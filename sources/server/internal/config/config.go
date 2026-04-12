package config

import (
	"errors"
	"fmt"
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
	PORT_DEFAULT = 5000
	PORT_MIN     = 0
	PORT_MAX     = 65535

	BUFSIZE_DEFAULT = 16

	SYS_CONF   = "/etc/volta/server.conf"
	LOCAL_CONF = "server.conf"
	ENV_FILE   = ".env"
)

type Config struct {
	ServerPort uint   `koanf:"port"`
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
	f.Uint("port", PORT_DEFAULT, "server port")
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
	if cfg.ServerPort > PORT_MAX || cfg.ServerPort < PORT_MIN {
		errs = append(errs, fmt.Errorf("invalid port %d, must be between %d and %d", cfg.ServerPort, PORT_MIN, PORT_MAX))
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
