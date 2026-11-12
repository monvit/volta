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

	"github.com/monvit/volta/server/internal/logger"
)

const (
	ADDR_DEFAULT              = "0.0.0.0"
	GRPCPORT_DEFAULT          = 5000
	RESTPORT_DEFAULT          = 8080
	BUFSIZE_DEFAULT           = 16
	LOGLEVEL_DEFAULT          = logger.INFO
	DEFAULT_DB_MAX_CONNS      = 8
	DEFAULT_DB_MAX_IDLE       = 2
	DEFAULT_DB_RETENTION_DAYS = 30

	SYS_CONF   = "/etc/volta/server.conf"
	LOCAL_CONF = "server.conf"
	ENV_CONF   = ".env"
)

var DefaultAllowedOrigins = []string{
	"http://localhost:5173",
	"http://127.0.0.1:5173",
	"https://monvit.github.io",
}

type Config struct {
	GRPC GRPCConfig   `koanf:"grpc"`
	REST RESTConfig   `koanf:"rest"`
	Buf  BufferConfig `koanf:"buf"`
	Log  LogConfig    `koanf:"log"`
	DB   DBConfig     `koanf:"db"`
}

type GRPCConfig struct {
	Addr string `koanf:"addr"`
	Port uint16 `koanf:"port"`
}

type RESTConfig struct {
	Addr           string   `koanf:"addr"`
	Port           uint16   `koanf:"port"`
	AllowedOrigins []string `koanf:"origins"`
}

type BufferConfig struct {
	Size uint `koanf:"size"`
}

type LogConfig struct {
	Level logger.Level `koanf:"level"`
}

type DBConfig struct {
	Enabled   bool   `koanf:"enabled"`
	DSN       string `koanf:"dsn"`
	MaxConns  int    `koanf:"max_conns"`
	MaxIdle   int    `koanf:"max_idle"`
	Retention int    `koanf:"retention_days"`
}

func Default() *Config {
	return &Config{
		GRPC: GRPCConfig{
			Addr: ADDR_DEFAULT,
			Port: GRPCPORT_DEFAULT,
		},
		REST: RESTConfig{
			Addr:           ADDR_DEFAULT,
			Port:           RESTPORT_DEFAULT,
			AllowedOrigins: append([]string(nil), DefaultAllowedOrigins...),
		},
		Buf: BufferConfig{
			Size: BUFSIZE_DEFAULT,
		},
		Log: LogConfig{
			Level: LOGLEVEL_DEFAULT,
		},
		DB: DBConfig{
			Enabled:   false,
			MaxConns:  DEFAULT_DB_MAX_CONNS,
			MaxIdle:   DEFAULT_DB_MAX_IDLE,
			Retention: DEFAULT_DB_RETENTION_DAYS,
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

	// .env
	if err := godotenv.Load(ENV_CONF); err != nil {
		if os.IsNotExist(err) {
			logger.Warn(".env file not found, skipping")
		} else {
			errs = append(errs, fmt.Errorf(".env: %w", err))
		}
	}

	// ENV → dot notation: only the first underscore separates the section from
	// the key, so DB_RETENTION_DAYS maps to db.retention_days (not db.retention.days).
	if err := k.Load(env.Provider("", ".", func(s string) string {
		s = strings.ToLower(s)
		if i := strings.IndexByte(s, '_'); i >= 0 {
			return s[:i] + "." + s[i+1:]
		}
		return s
	}), nil); err != nil {
		errs = append(errs, fmt.Errorf("env config: %w", err))
	}

	// FLAGS (SPÓJNE KLUCZE: DOT NOTATION)
	f := flag.NewFlagSet("config", flag.ContinueOnError)

	f.String("log.level", logger.LevelToString(LOGLEVEL_DEFAULT), "")
	f.String("grpc.addr", ADDR_DEFAULT, "")
	f.Uint("grpc.port", GRPCPORT_DEFAULT, "")
	f.String("rest.addr", ADDR_DEFAULT, "")
	f.Uint("rest.port", RESTPORT_DEFAULT, "")
	f.StringSlice("rest.origins", DefaultAllowedOrigins, "")
	f.Uint("buf.size", BUFSIZE_DEFAULT, "")

	f.Bool("db.enabled", false, "")
	f.String("db.dsn", "", "")
	f.Int("db.max_conns", DEFAULT_DB_MAX_CONNS, "")
	f.Int("db.max_idle", DEFAULT_DB_MAX_IDLE, "")
	f.Int("db.retention_days", DEFAULT_DB_RETENTION_DAYS, "")

	if err := f.Parse(os.Args[1:]); err != nil {
		errs = append(errs, fmt.Errorf("flags parse: %w", err))
	}

	// FLAGS → koanf (już bez transformacji)
	if err := k.Load(posflag.Provider(f, ".", k), nil); err != nil {
		errs = append(errs, fmt.Errorf("flags load: %w", err))
	}

	cfg := Default()

	if err := k.Unmarshal("", cfg); err != nil {
		errs = append(errs, fmt.Errorf("unmarshal: %w", err))
	}

	// validation
	if cfg.GRPC.Port == cfg.REST.Port {
		errs = append(errs, fmt.Errorf("gRPC and REST ports cannot be same"))
	}

	if cfg.DB.Enabled && cfg.DB.DSN == "" {
		errs = append(errs, fmt.Errorf("db.enabled requires db.dsn (or DB_DSN) to be set"))
	}

	if cfg.DB.MaxConns < 1 {
		cfg.DB.MaxConns = DEFAULT_DB_MAX_CONNS
	}
	if cfg.DB.MaxIdle < 0 {
		cfg.DB.MaxIdle = DEFAULT_DB_MAX_IDLE
	}
	if cfg.DB.Retention < 1 {
		cfg.DB.Retention = DEFAULT_DB_RETENTION_DAYS
	}

	if len(errs) > 0 {
		return nil, errors.Join(errs...)
	}

	return cfg, nil
}
