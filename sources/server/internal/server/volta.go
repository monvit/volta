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
	"google.golang.org/grpc/peer"
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

var (
	counter = 0
)

func (s *VoltaCollectorServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	p, ok := peer.FromContext(stream.Context())
	if !ok {
		return status.Errorf(codes.Internal, "peer info is not available")
	}

	log.Info("new connection from %v.", p.Addr.String())

	ch := make(chan *pb.ControlMessage, s.cfg.BufferSize)
	clientId := uuid.New().String()

	s.mu.Lock()
	// probably this check is not needed, but just in case
	if _, exists := s.clients[clientId]; exists {
		s.mu.Unlock()
		return status.Errorf(codes.AlreadyExists, "uuid collision")
	}

	s.clients[clientId] = &Client{
		stream: &stream,
		ch:     ch,
	}
	counter++

	s.mu.Unlock()

	log.Info("client %v with id %v connected", clientId, p.Addr.String())

	defer func() {
		s.mu.Lock()
		delete(s.clients, clientId)
		s.mu.Unlock()
		log.Info("client %v disconnected", clientId)
	}()

	g, ctx := errgroup.WithContext(stream.Context())

	g.Go(func() error {
		return s.recv(stream, ctx, &clientId)
	})
	g.Go(func() error {
		return s.send(stream, ch, ctx, &clientId)
	})
	g.Go(func() error {
		return s.ping(ch, ctx)
	})

	if err := g.Wait(); err != nil {
		log.Error("stream error: %v", err)
		return status.Errorf(codes.Internal, "stream error: %v", err)
	}

	return nil
}

func (s *VoltaCollectorServer) Clients() []string {
	return slices.Collect(maps.Keys(s.clients))
}

// TODO: check if client is connected, range should be passed as an argument
func (s *VoltaCollectorServer) RequestDataFromClient(clientId string) error {
	client, ok := s.clients[clientId]
	if !ok {
		return fmt.Errorf("client with given id %v doesn't exists", clientId)
	}

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
				return fmt.Errorf("recv: %w", err)
			}

			log.Debug("received message from client with id %v: %v", *clientId, msg)

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
				return fmt.Errorf("send: %w", err)
			}
		}
	}
}

func (s *VoltaCollectorServer) ping(ch chan<- *pb.ControlMessage, ctx context.Context) error {
	// TODO: configure interval, maybe random
	// send only if there are no recent messages from client, to avoid spamming?
	// remove client if there is no response to ping for a long time
	ticker := time.NewTicker(5 * time.Second)
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
	log.Debug("received message from client with id %v: %v", clientId, msg.Type.String())
	switch msg.Type {
	case pb.MessageType_PING:
		s.clients[clientId].ch <- CreateControlMessage(pb.MessageType_PONG)
	case pb.MessageType_PONG:
		// do nothing, just for testing connection
	case pb.MessageType_SEND_DATA:
		s.clients[clientId].ch <- CreateControlMessage(pb.MessageType_ERROR, WithError("SEND_DATA is only for clients"))
	case pb.MessageType_ERROR:
		log.Info("received error from client with id %v: %v", clientId, msg.GetError())
	case pb.MessageType_OK:
	default:
		log.Warn("unknown message type from client with id %v: %v", clientId, msg.Type)
	}
}
