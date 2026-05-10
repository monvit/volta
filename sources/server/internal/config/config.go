package config

import (
	"errors"
	"fmt"
	"net"
	"os"
	"strings"

	"github.com/knadh/koanf/parsers/toml"
	"github.com/knadh/koanf/providers/env"
	"github.com/knadh/koanf/providers/file"
	"github.com/knadh/koanf/providers/posflag"
	"github.com/knadh/koanf/v2"
	flag "github.com/spf13/pflag"

	log "github.com/monvit/volta/sources/server/internal/logger"
)

const (
	GRPCPORT_DEFAULT = 5000
	RESTPORT_DEFAULT = 8080
	PORT_MIN         = 0
	PORT_MAX         = 65535

	BUFSIZE_DEFAULT = 16

	SYS_CONF   = "/etc/volta/server.conf"
	LOCAL_CONF = "server.conf"
	ENV_FILE   = ".env"

	ADDR_DEFAULT = "localhost"
)

type Config struct {
	GRPCAddr   string `koanf:"grpc-addr"`
	GRPCPort   uint   `koanf:"grpc-port"`
	RESTAddr   string `koanf:"rest-addr"`
	RESTPort   uint   `koanf:"rest-port"`
	BufferSize uint   `koanf:"bufsize"`
	LogLevel   string `koanf:"log-level"`
}

type LoadResult struct {
	Config   *Config
	Warnings []error
}

func Load() (*LoadResult, error) {
	k := koanf.New(".")

	var errs []error
	var warnings []error

	// system config
	if err := k.Load(file.Provider(SYS_CONF), toml.Parser()); err != nil {
		if os.IsNotExist(err) {
			warnings = append(warnings, fmt.Errorf("sys config not found, skipping"))
		} else {
			warnings = append(warnings, fmt.Errorf("sys config: %w", err))
		}
	}

	// local config
	if err := k.Load(file.Provider(LOCAL_CONF), toml.Parser()); err != nil {
		if os.IsNotExist(err) {
			warnings = append(warnings, fmt.Errorf("local config not found, skipping"))
		} else {
			warnings = append(warnings, fmt.Errorf("local config: %w", err))
		}
	}

	// env
	if err := k.Load(env.Provider("", ".", func(s string) string {
		return strings.ToLower(strings.ReplaceAll(s, "_", "."))
	}), nil); err != nil {
		warnings = append(warnings, fmt.Errorf("env: %w", err))
	}

	// flags
	f := flag.NewFlagSet("config", flag.ContinueOnError)
	f.String("grpc-addr", ADDR_DEFAULT, "grpc server address")
	f.String("rest-addr", ADDR_DEFAULT, "rest server address")
	f.Uint("grpc-port", GRPCPORT_DEFAULT, "grpc server port")
	f.Uint("rest-port", RESTPORT_DEFAULT, "rest server port")
	f.Uint("bufsize", BUFSIZE_DEFAULT, "buffer size of each connection")
	f.String("log-level", "info", "log level (debug, info, warn, error)")

	if err := f.Parse(os.Args[1:]); err != nil {
		// incorrect flag
		return nil, fmt.Errorf("flags: %w", err)
	}

	if err := k.Load(posflag.Provider(f, ".", k), nil); err != nil {
		return nil, fmt.Errorf("flags: %w", err)
	}

	if len(k.Keys()) == 0 {
		return nil, fmt.Errorf("no configuration loaded (no config file, env, or flags)")
	}

	var cfg Config
	if err := k.Unmarshal("", &cfg); err != nil {
		return nil, fmt.Errorf("unmarshal: %w", err)
	}

	// validation
	if err := checkAddr(cfg.GRPCAddr); err != nil {
		errs = append(errs, fmt.Errorf("invalid gRPC address: %w", err))
	}

	if err := checkAddr(cfg.RESTAddr); err != nil {
		errs = append(errs, fmt.Errorf("invalid REST address: %w", err))
	}

	if cfg.GRPCPort == cfg.RESTPort {
		errs = append(errs, fmt.Errorf("gRPC and REST ports cannot be the same"))
	}

	if cfg.GRPCPort > PORT_MAX || cfg.GRPCPort < PORT_MIN {
		errs = append(errs, fmt.Errorf("invalid gRPC port %d, must be between %d and %d", cfg.GRPCPort, PORT_MIN, PORT_MAX))
	}

	if cfg.RESTPort > PORT_MAX || cfg.RESTPort < PORT_MIN {
		errs = append(errs, fmt.Errorf("invalid REST port %d, must be between %d and %d", cfg.RESTPort, PORT_MIN, PORT_MAX))
	}

	lvl, err := log.ParseLevel(cfg.LogLevel)
	if err != nil {
		warnings = append(warnings, fmt.Errorf("log-level %q unknown, using INFO", cfg.LogLevel))
		log.SetLevel(log.INFO)
	} else {
		log.SetLevel(lvl)
	}

	if len(errs) > 0 {
		return nil, errors.Join(errs...)
	}

	return &LoadResult{
		Config:   &cfg,
		Warnings: warnings,
	}, nil
}

func checkAddr(addr string) error {
	if net.ParseIP(addr) != nil {
		return nil
	}
	if _, err := net.LookupHost(addr); err != nil {
		return fmt.Errorf("invalid address: %s", addr)
	}
	return nil
}
