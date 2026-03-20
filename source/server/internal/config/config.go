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
)

const (
	PORT_DEFAULT = 5000
	PORT_MIN     = 0
	PORT_MAX     = 65535
	SYS_CONF     = "/etc/volta/server.conf"
	LOCAL_CONF   = "server.conf"
	ENV_FILE     = ".env"
)

type Config struct {
	ServerPort int `koanf:"port"`
}

func Load() (*Config, error) {
	k := koanf.New(".")

	var errs []error

	// system config
	if err := k.Load(file.Provider(SYS_CONF), toml.Parser()); err != nil {
		if !os.IsNotExist(err) {
			errs = append(errs, fmt.Errorf("sys config: %w", err))
		}
	}

	// local config
	if err := k.Load(file.Provider(LOCAL_CONF), toml.Parser()); err != nil {
		if !os.IsNotExist(err) {
			errs = append(errs, fmt.Errorf("local config: %w", err))
		}
	}

	// env
	if err := k.Load(env.Provider("", ".", func(s string) string {
		return strings.ToLower(strings.ReplaceAll(s, "_", "."))
	}), nil); err != nil {
		errs = append(errs, fmt.Errorf("env: %w", err))
	}

	// flags
	f := flag.NewFlagSet("config", flag.ContinueOnError)
	f.Int("port", PORT_DEFAULT, "server port")

	if err := f.Parse(os.Args[1:]); err != nil {
		return nil, fmt.Errorf("flags: %w", err)
	}

	if err := k.Load(posflag.Provider(f, ".", k), nil); err != nil {
		errs = append(errs, fmt.Errorf("flags: %w", err))
	}

	var cfg Config
	if err := k.Unmarshal("", &cfg); err != nil {
		return nil, fmt.Errorf("unmarshal: %w", err)
	}

	// validation
	if cfg.ServerPort < PORT_MIN || cfg.ServerPort > PORT_MAX {
		return nil, fmt.Errorf("invalid port: %d, should be <%d, %d>", cfg.ServerPort, PORT_MIN, PORT_MAX)
	}

	if len(k.Keys()) == 0 {
		return nil, fmt.Errorf("no config loaded: %w", errors.Join(errs...))
	}

	if len(errs) > 0 {
		return &cfg, fmt.Errorf("config loaded with warnings: %w", errors.Join(errs...))
	}

	return &cfg, nil
}
