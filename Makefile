proto-go:
	protoc -I libs/proto \
	  --go_out=source\
	  --go-grpc_out=source\
	  libs/proto/volta.proto

server:
	cd source/server && go build

server-run:
	cd source/server && go run main.go
