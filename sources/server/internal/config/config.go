package config

import (
	"errors"
	"fmt"
	"os"
	"strings"

	"github.com/joho/godotenv"
	"github.com/knadh/koanf/parsers/toml"
	"github.com/knadh/koanf/providers/env"
	"github.com/knadh/koanf/providers/file"
	"github.com/knadh/koanf/providers/posflag"
	"github.com/knadh/koanf/v2"
	flag "github.com/spf13/pflag"

	"github.com/monvit/volta/sources/server/internal/logger"
)

const (
	ADDR_DEFAULT     = "0.0.0.0"
	GRPCPORT_DEFAULT = 5000
	RESTPORT_DEFAULT = 8080
	BUFSIZE_DEFAULT  = 16
	LOGLEVEL_DEFAULT = logger.INFO

	SYS_CONF   = "/etc/volta/server.conf"
	LOCAL_CONF = "server.conf"
	ENV_CONF   = ".env"
)

type Config struct {
	GRPC GRPCConfig   `koanf:"grpc"`
	REST RESTConfig   `koanf:"rest"`
	Buf  BufferConfig `koanf:"buf"`
	Log  LogConfig    `koanf:"log"`
}

type GRPCConfig struct {
	Addr string `koanf:"addr"`
	Port uint16 `koanf:"port"`
}

type RESTConfig struct {
	Addr string `koanf:"addr"`
	Port uint16 `koanf:"port"`
}

type BufferConfig struct {
	Size uint `koanf:"size"`
}

type LogConfig struct {
	Level logger.Level `koanf:"level"`
}

func Default() *Config {
	return &Config{
		GRPC: GRPCConfig{
			Addr: ADDR_DEFAULT,
			Port: GRPCPORT_DEFAULT,
		},
		REST: RESTConfig{
			Addr: ADDR_DEFAULT,
			Port: RESTPORT_DEFAULT,
		},
		Buf: BufferConfig{
			Size: BUFSIZE_DEFAULT,
		},
		Log: LogConfig{
			Level: LOGLEVEL_DEFAULT,
		},
	}
}

func Load() (*Config, error) {
	k := koanf.New(".")

	var errs []error

	// system config
	if err := k.Load(file.Provider(SYS_CONF), toml.Parser()); err != nil {
		if os.IsNotExist(err) {
			logger.Warn("sys config not found, skipping")
		} else {
			errs = append(errs, fmt.Errorf("sys config: %w", err))
		}
	}

	// local config
	if err := k.Load(file.Provider(LOCAL_CONF), toml.Parser()); err != nil {
		if os.IsNotExist(err) {
			logger.Warn("local config not found, skipping")
		} else {
			errs = append(errs, fmt.Errorf("local config: %w", err))
		}
	}

	// loading .env
	if err := godotenv.Load(ENV_CONF); err != nil {
		if os.IsNotExist(err) {
			logger.Warn(".env file not found, skipping")
		} else {
			errs = append(errs, fmt.Errorf(".env: %w", err))
		}
	}

	// env
	if err := k.Load(env.Provider("", ".", func(s string) string {
		return strings.ToLower(strings.ReplaceAll(s, "_", "."))
	}), nil); err != nil {
		errs = append(errs, fmt.Errorf("env config: %w", err))
	}

	// flags
	f := flag.NewFlagSet("config", flag.ContinueOnError)

	f.String("grpc-addr", ADDR_DEFAULT, "")
	f.Uint("grpc-port", GRPCPORT_DEFAULT, "")
	f.String("rest-addr", ADDR_DEFAULT, "")
	f.Uint("rest-port", RESTPORT_DEFAULT, "")
	f.Uint("buf-size", BUFSIZE_DEFAULT, "")
	f.String("log-level", logger.LevelToString(LOGLEVEL_DEFAULT), "")

	if err := f.Parse(os.Args[1:]); err != nil {
		errs = append(errs, fmt.Errorf("flags: %w", err))
	}

	if err := k.Load(posflag.Provider(f, ".", k), nil); err != nil {
		errs = append(errs, fmt.Errorf("flags: %w", err))
	}

	cfg := Default()

	if err := k.Unmarshal("", cfg); err != nil {
		errs = append(errs, fmt.Errorf("unmarshal: %w", err))
	}

	// validation
	if cfg.GRPC.Port == cfg.REST.Port {
		errs = append(errs, fmt.Errorf("gRPC and REST ports cannot be same"))
	}

	if len(errs) > 0 {
		return nil, errors.Join(errs...)
	}

	return cfg, nil
}
