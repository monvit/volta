package server

import (
	"fmt"
	"io"
	"maps"
	"slices"
	"sync"

	cfg "github.com/monvit/volta/sources/server/internal/config"
	log "github.com/monvit/volta/sources/server/internal/logger"
	pb "github.com/monvit/volta/sources/server/pb"

	"google.golang.org/grpc/peer"
)

type Client struct {
	stream *pb.VoltaCollector_ConnectServer
	ch     chan *pb.ControlMessage
}

type VoltaCollectorServer struct {
	pb.UnimplementedVoltaCollectorServer

	mu      sync.Mutex
	clients map[string]*Client
}

func (s *VoltaCollectorServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	p, ok := peer.FromContext(stream.Context())
	if !ok {
		return fmt.Errorf("no peer info available")
	}

	log.Debug("new connection from %v created.", p.Addr.String())

	ch := make(chan *pb.ControlMessage, cfg.BUFSIZE_DEFAULT)

	// TODO: better key
	s.mu.Lock()
	s.clients[p.Addr.String()] = &Client{
		stream: &stream,
		ch:     ch,
	}
	s.mu.Unlock()

	var wg sync.WaitGroup
	wg.Add(2)

	go s.read(stream, &wg)
	go s.write(stream, ch, &wg)

	wg.Wait()

	return nil
}

func (s *VoltaCollectorServer) Clients() []string {
	return slices.Collect(maps.Keys(s.clients))
}

func (s *VoltaCollectorServer) RequestDataFromClient(clientKey string) error {
	client, ok := s.clients[clientKey]
	if !ok {
		return fmt.Errorf("client with given key %v doesn't exists", clientKey)
	}

	client.ch <- &pb.ControlMessage{
		Type:    pb.MessageType_SEND_DATA,
		Payload: "",
	}

	return nil
}

func (s *VoltaCollectorServer) read(stream pb.VoltaCollector_ConnectServer, wg *sync.WaitGroup) error {
	defer wg.Done()
	for {
		msg, err := stream.Recv()
		if err == io.EOF {
			log.Info("read stream closed")
			return nil
		}

		if err != nil {
			log.Error("error while reading: %v\n\n", err.Error())
			return err
		}

		log.Debug("received message: %v\n", msg)
	}
}

func (s *VoltaCollectorServer) write(stream pb.VoltaCollector_ConnectServer, ch chan *pb.ControlMessage, wg *sync.WaitGroup) error {
	defer wg.Done()
	for msg := range ch {
		err := stream.Send(msg)
		if err == io.EOF {
			log.Info("write stream closed")
			return nil
		}

		if err != nil {
			log.Error("error while writing: %v", err.Error())
			return err
		}
	}

	return nil
}
