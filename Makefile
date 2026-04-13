proto-go:
	protoc -I sources/proto \
	  --go_out=sources\
	  --go-grpc_out=sources\
	  sources/proto/volta.proto

server:
	cd sources/server/cmd/server && go build

server-run:
	cd sources/server && go run cmd/server/main.go

hooks:
	chmod +x scripts/hooks/pre-commit
	cd scripts/hooks && ln -sfr ./pre-commit ../../.git/hooks/pre-commit