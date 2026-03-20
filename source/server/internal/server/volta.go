package server

import (
	"context"
	"fmt"
	"io"
	"log"
	"strconv"
	"sync"
	pb "volta/server/generated/volta"
)

type VoltaCollectorServer struct {
	pb.UnimplementedVoltaCollectorServer
	mu sync.Mutex
}

func (s *VoltaCollectorServer) SendMessage(ctx context.Context, request *pb.Message) (*pb.Response, error) {
	log.Printf("Request: %s", request.Message)
	response := pb.Response{}
	if request.Message == "Hello, World!" {
		response.Response = "Hello!"
	} else {
		response.Response = "Who's there?"
	}

	return &response, nil
}

func (s *VoltaCollectorServer) SendMessages(stream pb.VoltaCollector_SendMessagesServer) error {
	var count int
	for {
		message, err := stream.Recv()
		if err == io.EOF {
			var response pb.Response
			response.Response = "got " + strconv.Itoa(count) + " messages"
			return stream.SendAndClose(&response)
		}

		if err != nil {
			return err
		}

		count++

		fmt.Println(message.Message)
	}
}

func (s *VoltaCollectorServer) GetResponses(message *pb.Message, stream pb.VoltaCollector_GetResponsesServer) error {
	fmt.Println(message.Message)

	for i := range 10 {
		var response pb.Response
		response.Response = strconv.Itoa(i)
		err := stream.Send(&response)

		if err != nil {
			return err
		}
	}

	return nil
}

func (s *VoltaCollectorServer) Talk(stream pb.VoltaCollector_TalkServer) error {
	var count int
	for {
		msg, err := stream.Recv()
		if err != nil {
			return err
		}
		fmt.Println(msg)
		count++
		var response pb.Response
		response.Response = "response " + strconv.Itoa(count)
		err = stream.Send(&response)

		if err != nil {
			return err
		}
	}
}
