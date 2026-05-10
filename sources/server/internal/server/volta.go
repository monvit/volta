package server

import (
	"context"
	"fmt"
	"io"
	"maps"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/monvit/volta/sources/server/internal/broker"
	config "github.com/monvit/volta/sources/server/internal/config"
	log "github.com/monvit/volta/sources/server/internal/logger"
	t "github.com/monvit/volta/sources/server/internal/types"
	pb "github.com/monvit/volta/sources/server/pb"
	pbt "github.com/monvit/volta/sources/server/pb/types"
	"golang.org/x/sync/errgroup"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

type rpcerror struct {
	err  error
	code codes.Code
}

func (e *rpcerror) Error() string {
	return e.err.Error()
}

func rpcErrorf(code codes.Code, format string, a ...any) *rpcerror {
	return &rpcerror{
		err:  status.Errorf(code, format, a...),
		code: code,
	}
}

type Client struct {
	t.Agent
	stream   *pb.VoltaCollector_ConnectServer
	ch       chan *pbt.ControlMessage
	lastPong time.Time
}

type VoltaCollectorServer struct {
	pb.UnimplementedVoltaCollectorServer

	mu      sync.RWMutex
	clients map[string]*Client
	cfg     *config.Config
	broker  *broker.Broker
}

func New(cfg *config.Config, broker *broker.Broker) *VoltaCollectorServer {
	return &VoltaCollectorServer{
		clients: make(map[string]*Client),
		cfg:     cfg,
		broker:  broker,
	}
}

func (s *VoltaCollectorServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	md, ok := metadata.FromIncomingContext(stream.Context())
	if !ok {
		return status.Errorf(codes.Internal, "metadata is not available")
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

		log.Infof("(Connect) connection from existing id %v.", id)
	}

	if err := s.addClient(id, &stream); err != nil {
		return err
	}

	client := s.clients[id]
	client.ch <- CreateControlMessage(pbt.MessageType_MESSAGE_ID, WithPayload(id))

	defer func() {
		s.mu.Lock()
		delete(s.clients, id)
		s.mu.Unlock()
		log.Infof("(Connect) client %v disconnected", id)
	}()

	// client.ch <- CreateControlMessage(pbt.MessageType_STREAM_DATA)

	g, ctx := errgroup.WithContext(stream.Context())

	g.Go(func() error {
		return s.recv(stream, ctx, client)
	})
	g.Go(func() error {
		return s.send(stream, client.ch, ctx, &client.Id)
	})
	g.Go(func() error {
		return s.ping(client.ch, ctx)
	})

	if err := g.Wait(); err != nil {
		log.Errorf("(Connect) stream error: %v", err)
		return status.Errorf(codes.Internal, "stream error: %v", err)
	}

	return nil
}

func (s *VoltaCollectorServer) addClient(id string, stream *pb.VoltaCollector_ConnectServer) *rpcerror {
	if _, exists := s.clients[id]; exists {
		return rpcErrorf(codes.AlreadyExists, "client with id %v already exists", id)
	}

	s.mu.Lock()

	s.clients[id] = &Client{
		Agent: t.Agent{
			Id:     id,
			Status: t.AGENT_STATUS_CONNECTED,
		},
		stream: stream,
		ch:     make(chan *pbt.ControlMessage, s.cfg.BufferSize),
	}

	s.mu.Unlock()

	return nil
}

func (s *VoltaCollectorServer) recv(stream pb.VoltaCollector_ConnectServer, ctx context.Context, client *Client) error {
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
				return fmt.Errorf("recv [%v]: %w", client.Id, err)
			}

			log.Debugf("(Connect) received message from client with id %v: %v", client.Id, msg)

			client.lastPong = time.Now()
			s.handleMessage(msg, client)
		}
	}
}

func (s *VoltaCollectorServer) send(stream pb.VoltaCollector_ConnectServer, ch <-chan *pbt.ControlMessage, ctx context.Context, clientId *string) error {
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case msg, ok := <-ch:
			if !ok {
				return nil
			}
			if err := stream.Send(msg); err != nil {
				return fmt.Errorf("(Connect) [%v] send: %v", &clientId, err)
			}
		}
	}
}

func (s *VoltaCollectorServer) ping(ch chan<- *pbt.ControlMessage, ctx context.Context) error {
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
			ch <- CreateControlMessage(pbt.MessageType_MESSAGE_PING)
		}
	}
}

func (s *VoltaCollectorServer) handleMessage(msg *pbt.ControlMessage, client *Client) {
	switch msg.Type {
	case pbt.MessageType_MESSAGE_PING:
		client.ch <- CreateControlMessage(pbt.MessageType_MESSAGE_PONG)
	case pbt.MessageType_MESSAGE_PONG:
		// TODO
	case pbt.MessageType_MESSAGE_SEND_DATA:
		client.ch <- CreateControlMessage(pbt.MessageType_MESSAGE_ERROR, WithPayload("SEND_DATA is only for clients"))
	case pbt.MessageType_MESSAGE_STREAM_DATA:
		client.ch <- CreateControlMessage(pbt.MessageType_MESSAGE_ERROR, WithPayload("STREAM_DATA is only for clients"))
	case pbt.MessageType_MESSAGE_ERROR:
		log.Infof("received error from id %v: %v", client.Id, msg.GetPayload())
	case pbt.MessageType_MESSAGE_OK:
	default:
		log.Warnf("unknown message type from id %v: %v", client.Id, msg.Type)
	}
}

func (s *VoltaCollectorServer) StreamData(stream pb.VoltaCollector_StreamDataServer) error {
	ctx := stream.Context()
	id, err := s.checkId(ctx)
	if err != nil {
		return err
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
				return status.Errorf(codes.Internal, "recv: %v", err)
			}

			s.broker.Publish(id, msg)

			log.Debugf("(StreamData) received metric from id %v: %v", id, msg)
		}
	}
}

func (s *VoltaCollectorServer) checkId(ctx context.Context) (string, *rpcerror) {
	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return "", rpcErrorf(codes.Internal, "metadata is not available")
	}

	ids := md.Get("id")
	if len(ids) == 0 || ids[0] == "" {
		return "", rpcErrorf(codes.InvalidArgument, "client id is required in metadata")
	}

	id := ids[0]
	if _, exists := s.clients[id]; !exists {
		return "", rpcErrorf(codes.InvalidArgument, "client with id: %v, doesn't exist", id)
	}

	return id, nil
}

func (s *VoltaCollectorServer) ListAgents() []t.Agent {
	s.mu.RLock()
	defer s.mu.RUnlock()

	agents := make([]t.Agent, len(s.clients))

	for client := range maps.Values(s.clients) {
		agents = append(agents, client.Agent)
	}

	return agents
}

func (s *VoltaCollectorServer) RequestStreamMetrics(agentId string) error {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if client, exists := s.clients[agentId]; exists {
		client.ch <- CreateControlMessage(pbt.MessageType_MESSAGE_STREAM_DATA)
		return nil
	}

	return fmt.Errorf("client with id: %v, doesn't exist", agentId)
}

func (s *VoltaCollectorServer) RequestSendMetrics(agentId string) error {
	s.mu.RLock()
	defer s.mu.RUnlock()

	if client, exists := s.clients[agentId]; exists {
		client.ch <- CreateControlMessage(pbt.MessageType_MESSAGE_SEND_DATA)
		return nil
	}

	return fmt.Errorf("client with id: %v, doesn't exist", agentId)
}
