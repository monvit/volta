package logger

import (
	"fmt"
	"io"
	"os"
	"strings"
	"sync"
	"time"
)

const (
	reset  = "\033[0m"
	red    = "\033[31m"
	yellow = "\033[33m"
	blue   = "\033[34m"
	gray   = "\033[90m"
)

type Level uint

const (
	DEBUG Level = iota
	INFO
	WARN
	ERROR
)

// UnmarshalText implements encoding.TextUnmarshaler for parsing log level from config
func (l *Level) UnmarshalText(text []byte) error {
	level, err := ParseLevel(string(text))
	if err != nil {
		return err
	}
	*l = level
	return nil
}

func ParseLevel(s string) (Level, error) {
	switch strings.ToLower(s) {
	case "debug":
		return DEBUG, nil
	case "info":
		return INFO, nil
	case "warn", "warning":
		return WARN, nil
	case "error":
		return ERROR, nil
	default:
		return INFO, fmt.Errorf("unknown log level: %s", s)
	}
}

func LevelToString(l Level) string {
	switch l {
	case DEBUG:
		return "debug"
	case INFO:
		return "info"
	case WARN:
		return "warn"
	case ERROR:
		return "error"
	default:
		return "info"
	}
}

var (
	level   = INFO
	fileOut io.Writer
	mu      sync.Mutex
)

func SetLevel(l Level) {
	level = l
}

func Init() error {
	if err := os.MkdirAll("logs", 0755); err != nil {
		return fmt.Errorf("cannot create logs dir: %w", err)
	}

	filename := fmt.Sprintf("logs/%s.log", time.Now().Format("2006-01-02_15-04-05"))
	f, err := os.OpenFile(filename, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0644)
	if err != nil {
		return fmt.Errorf("cannot open file %s: %w", filename, err)
	}

	fileOut = f
	return nil
}

func log(color, label, format string, args ...any) {
	ts := time.Now().Format("15:04:05")
	msg := format
	if len(args) > 0 {
		msg = fmt.Sprintf(format, args...)
	}

	mu.Lock()
	defer mu.Unlock()

	fmt.Fprintf(os.Stderr, "%s%s%s %s%s%s %s\n",
		gray, ts, reset,
		color, label, reset,
		msg,
	)

	if fileOut != nil {
		fmt.Fprintf(fileOut, "%s %s %s\n", ts, label, msg)
	}
}

func Debug(format string, args ...any) {
	if level <= DEBUG {
		log(gray, "[DBG]", format, args...)
	}
}

func Info(format string, args ...any) {
	if level <= INFO {
		log(blue, "[INF]", format, args...)
	}
}

func Warn(format string, args ...any) {
	if level <= WARN {
		log(yellow, "[WRN]", format, args...)
	}
}

func Error(format string, args ...any) {
	if level <= ERROR {
		log(red, "[ERR]", format, args...)
	}
}
