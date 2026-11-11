PROTO_DIR = ../proto
OUT_DIR   = ./pb

PROTO_FILES = $(shell find $(PROTO_DIR) -name "*.proto")

PROTOC_INCLUDES = -I $(PROTO_DIR)
ifneq ($(wildcard /usr/include/google/protobuf),)
PROTOC_INCLUDES += -I /usr/include
endif
ifneq ($(wildcard /usr/local/include/google/protobuf),)
PROTOC_INCLUDES += -I /usr/local/include
endif
ifneq ($(wildcard /opt/homebrew/include/google/protobuf),)
PROTOC_INCLUDES += -I /opt/homebrew/include
endif

proto:
	mkdir -p $(OUT_DIR)
	protoc $(PROTOC_INCLUDES) \
	  --go_out=paths=source_relative:$(OUT_DIR) \
	  --go-grpc_out=paths=source_relative:$(OUT_DIR) \
	  $(PROTO_FILES)

build:
	go build

run:
	go run ./cmd/server

db-migrate:
	go run ./cmd/migrate up

db-migrate-down:
	go run ./cmd/migrate down

db-status:
	go run ./cmd/migrate status

db-prereq:
	../../scripts/db/prereq.sh

db-init:
	../../scripts/db/init_db.sh
