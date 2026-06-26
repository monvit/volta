PROTO_DIR = ../proto
OUT_DIR   = ./pb

PROTO_FILES = $(shell find $(PROTO_DIR) -name "*.proto")

proto:
	mkdir -p $(OUT_DIR)
	protoc -I $(PROTO_DIR) \
	  --go_out=paths=source_relative:$(OUT_DIR) \
	  --go-grpc_out=paths=source_relative:$(OUT_DIR) \
	  $(PROTO_FILES)

build:
	go build

run:
	go run ./cmd/server
