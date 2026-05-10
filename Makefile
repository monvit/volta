PROTO_DIR = sources/proto
OUT_DIR   = sources/server/pb

PROTO_FILES = $(shell find $(PROTO_DIR) -name "*.proto")

proto-go:
	mkdir -p $(OUT_DIR)
	protoc -I $(PROTO_DIR) \
	  --go_out=paths=source_relative:$(OUT_DIR) \
	  --go-grpc_out=paths=source_relative:$(OUT_DIR) \
	  $(PROTO_FILES)

server:
	cd sources/server/cmd/server && go build

server-run:
	cd sources/server && go run cmd/server/main.go

hooks:
	chmod +x scripts/hooks/pre-commit
	cd scripts/hooks && ln -sfr ./pre-commit ../../.git/hooks/pre-commit

build-agent-base:
	docker build -t volta/agent-base:latest -f docker/agent/Dockerfile.base --no-cache sources/agent

docker-dev:
	docker compose -f docker/docker-compose.dev.yml up --build --watch