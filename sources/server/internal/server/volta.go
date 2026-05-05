package server

import (
	"context"
	"fmt"
	"io"
	"maps"
	"slices"
	"sync"
	"time"

	"github.com/google/uuid"
	config "github.com/monvit/volta/sources/server/internal/config"
	log "github.com/monvit/volta/sources/server/internal/logger"
	pb "github.com/monvit/volta/sources/server/pb"
	"golang.org/x/sync/errgroup"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

type Client struct {
	stream   *pb.VoltaCollector_ConnectServer
	ch       chan *pb.ControlMessage
	lastPong time.Time
	// NOTE: don't know if needed
	// mu       sync.RWMutex
}

type VoltaCollectorServer struct {
	pb.UnimplementedVoltaCollectorServer

	mu      sync.Mutex
	clients map[string]*Client
	cfg     *config.Config
}

func (s *VoltaCollectorServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	md, ok := metadata.FromIncomingContext(stream.Context())
	if !ok {
		return status.Errorf(codes.Internal, "(Connect) metadata is not available")
	}

	ids := md.Get("id")
	var id string

	if len(ids) == 0 || (len(ids) > 0 && ids[0] == "") {
		// generate new id for new client
		id = uuid.New().String()

		log.Infof("(Connect) connection from new client, assigned id %v.", id)
	} else {
		// client with existing id
		id = ids[0]
		// NOTE: below check will be relevant when we implement the database
		// if _, exists := s.clients[id]; !exists {
		// 	return status.Errorf(codes.InvalidArgument, "client with id: %v doesn't exist", id)
		// }

		log.Infof("(Connect) connection from existing id %v.", id)
	}

	s.mu.Lock()
	// probably this check is not needed, but just in case
	if _, exists := s.clients[id]; exists {
		s.mu.Unlock()
		log.Errorf("(Connect) uuid collision when creating client id: %v", id)
		return status.Errorf(codes.AlreadyExists, "(Connect) uuid collision when creating client id")
	}

	s.clients[id] = &Client{
		stream: &stream,
		ch:     make(chan *pb.ControlMessage, s.cfg.BufferSize),
	}

	s.clients[id].ch <- CreateControlMessage(pb.MessageType_ID, WithPayload(id))

	s.mu.Unlock()

	defer func() {
		s.mu.Lock()
		delete(s.clients, id)
		s.mu.Unlock()
		log.Infof("(Connect) client %v disconnected", id)
	}()

	s.clients[id].ch <- CreateControlMessage(pb.MessageType_STREAM_DATA)

	g, ctx := errgroup.WithContext(stream.Context())

	g.Go(func() error {
		return s.recv(stream, ctx, &id)
	})
	g.Go(func() error {
		return s.send(stream, s.clients[id].ch, ctx, &id)
	})
	g.Go(func() error {
		return s.ping(s.clients[id].ch, ctx)
	})

	if err := g.Wait(); err != nil {
		log.Errorf("(Connect) stream error: %v", err)
		return status.Errorf(codes.Internal, "(Connect) stream error: %v", err)
	}

	return nil
}

func (s *VoltaCollectorServer) StreamData(stream pb.VoltaCollector_StreamDataServer) error {
	// NOTE: maybe move id checking to separate function, since it will be needed in other RPCs
	ctx := stream.Context()
	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return status.Errorf(codes.Internal, "(StreamData) metadata is not available")
	}

	ids := md.Get("id")
	if len(ids) == 0 || ids[0] == "" {
		return status.Errorf(codes.InvalidArgument, "(StreamData) client id is required in metadata")
	}

	id := ids[0]
	if _, exists := s.clients[id]; !exists {
		return status.Errorf(codes.InvalidArgument, "(StreamData) client with id: %v, doesn't exist", id)
	}

	log.Infof("(StreamData) client with id %v started streaming data", id)

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
			msg, err := stream.Recv()
			if err == io.EOF {
				return nil
			}
			if err != nil {
				return fmt.Errorf("(StreamData) recv: %w", err)
			}

			log.Debugf("(StreamData) received metric from id %v: %v", id, msg)
		}
	}
}

func (s *VoltaCollectorServer) Clients() []string {
	return slices.Collect(maps.Keys(s.clients))
}

// TODO: check if client is connected, range should be passed as an argument
func (s *VoltaCollectorServer) RequestDataFromClient(clientId string) error {
	client, ok := s.clients[clientId]
	if !ok {
		return fmt.Errorf("(RequestDataFromClient) client with given id %v, doesn't exist", clientId)
	}

	// TODO: add options to control message, e.g. count, time range, etc.
	client.ch <- CreateControlMessage(pb.MessageType_SEND_DATA, WithCount(10))

	return nil
}

func (s *VoltaCollectorServer) recv(stream pb.VoltaCollector_ConnectServer, ctx context.Context, clientId *string) error {
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
			msg, err := stream.Recv()
			if err == io.EOF {
				return nil
			}
			if err != nil {
				return fmt.Errorf("(Connect) recv: %w", err)
			}

			log.Debugf("(Connect) received message from client with id %v: %v", *clientId, msg)

			s.clients[*clientId].lastPong = time.Now()
			s.handleMessage(msg, *clientId)
		}
	}
}

func (s *VoltaCollectorServer) send(stream pb.VoltaCollector_ConnectServer, ch <-chan *pb.ControlMessage, ctx context.Context, clientId *string) error {
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case msg, ok := <-ch:
			if !ok {
				return nil
			}
			if err := stream.Send(msg); err != nil {
				return fmt.Errorf("(Connect) send: %w", err)
			}
		}
	}
}

func (s *VoltaCollectorServer) ping(ch chan<- *pb.ControlMessage, ctx context.Context) error {
	// TODO: configure interval, maybe random
	// send only if there are no recent messages from client, to avoid spamming?
	// remove client if there is no response to ping for a long time
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
			ch <- CreateControlMessage(pb.MessageType_PING)
		}
	}
}

func (s *VoltaCollectorServer) handleMessage(msg *pb.ControlMessage, clientId string) {
	switch msg.Type {
	case pb.MessageType_PING:
		s.clients[clientId].ch <- CreateControlMessage(pb.MessageType_PONG)
	case pb.MessageType_PONG:
		// do nothing, just for testing connection
	case pb.MessageType_SEND_DATA:
		s.clients[clientId].ch <- CreateControlMessage(pb.MessageType_ERROR, WithPayload("SEND_DATA is only for clients"))
	case pb.MessageType_ERROR:
		log.Infof("received error from id %v: %v", clientId, msg.GetPayload())
	case pb.MessageType_OK:
	default:
		log.Warnf("unknown message type from id %v: %v", clientId, msg.Type)
	}
}
