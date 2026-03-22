package server

import (
	"io"
	"log"
	"strconv"
	"sync"
	pb "volta/server/generated/volta"
)

type VoltaCollectorServer struct {
	pb.UnimplementedVoltaCollectorServer
	mu             sync.Mutex
	messagesToSend chan *pb.ControlMessage
}

func (s *VoltaCollectorServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	log.Printf("New connection created.")

	for i := range 10 {
		var msg pb.ControlMessage
		msg.Type = pb.MessageType_SEND_DATA
		msg.Payload = "message: " + strconv.FormatInt(int64(i), 10)
		s.messagesToSend <- &msg
	}

	var wg sync.WaitGroup
	wg.Add(2)

	go s.read(stream, &wg)
	go s.write(stream, &wg)

	wg.Wait()

	return nil
}

func (s *VoltaCollectorServer) read(stream pb.VoltaCollector_ConnectServer, wg *sync.WaitGroup) error {
	defer wg.Done()
	for {
		msg, err := stream.Recv()
		if err == io.EOF {
			log.Println("[READ]: Stream closed")
			return nil
		}
		if err != nil {
			log.Printf("[READ]: Error while reading: %v\n\n", err.Error())
			return err
		}

		log.Printf("[READ]: Received message: %v\n", msg)
	}
}

func (s *VoltaCollectorServer) write(stream pb.VoltaCollector_ConnectServer, wg *sync.WaitGroup) error {
	defer wg.Done()
	for msg := range s.messagesToSend {
		err := stream.Send(msg)
		if err == io.EOF {
			log.Println("[WRITE]: Stream closed")
			return nil
		}
		if err != nil {
			log.Printf("[WRITE] Error while writing: %v", err.Error())
			return err
		}
	}

	return nil
}
