FROM golang:1.25-bookworm AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends protobuf-compiler libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src/sources/server

COPY sources/server/go.mod sources/server/go.sum ./
RUN go mod download

COPY sources/proto ../proto
COPY sources/server ./

ENV PATH="${PATH}:$(go env GOPATH)/bin"
RUN go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11 \
    && go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.5.1 \
    && make proto \
    && CGO_ENABLED=0 go build -o /out/volta-server ./cmd/server \
    && CGO_ENABLED=0 go build -o /out/volta-migrate ./cmd/migrate

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /var/lib/volta && chown nobody:nogroup /var/lib/volta

COPY --from=build /out/volta-server /usr/local/bin/volta-server
COPY --from=build /out/volta-migrate /usr/local/bin/volta-migrate

USER nobody
WORKDIR /var/lib/volta

EXPOSE 5000 8080
CMD ["volta-server"]