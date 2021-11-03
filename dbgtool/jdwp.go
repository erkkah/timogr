package main

import (
	"fmt"
	"net"
	"time"
)

type JDWP struct {
	port int
	conn net.Conn
}

func NewJDWP(port int) JDWP {
	return JDWP{
		port: port,
	}
}

func (jc *JDWP) Connect() error {
	var err error

	jc.conn, err = net.DialTimeout("tcp", fmt.Sprintf("localhost:%d", jc.port), time.Second*5)
	if err != nil {
		return err
	}

	jc.conn.SetDeadline(time.Now().Add(time.Second * 5))

	handshakeMessage := "JDWP-Handshake"
	_, err = jc.conn.Write([]byte(handshakeMessage))
	if err != nil {
		return fmt.Errorf("failed to send handshake: %w", err)
	}

	handshake := make([]byte, len(handshakeMessage))
	_, err = jc.conn.Read(handshake)
	if err != nil {
		return fmt.Errorf("failed to read handshake response: %w", err)
	}

	if handshakeMessage != string(handshake) {
		return fmt.Errorf("failed handshake")
	}

	return nil
}

func (jc *JDWP) Resume() error {
	resumeMessage := []byte{0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x09}
	_, err := jc.conn.Write(resumeMessage)
	if err != nil {
		return fmt.Errorf("resume request failed: %w", err)
	}

	reply := make([]byte, 11)
	_, err = jc.conn.Read(reply)
	if err != nil || reply[9] != 0 || reply[10] != 0 {
		return fmt.Errorf("resume failed")
	}

	return nil
}

func (jc *JDWP) Close() error {
	return jc.conn.Close()
}
