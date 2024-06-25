package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"log"
	"net"
	"time"

	"github.com/alexflint/go-arg"
	"google.golang.org/protobuf/proto"

	"tobycm.dev/audio-share-go-client/pb"
)

var audioFormat = &pb.AudioFormat{}

var args struct {
	Host string `arg:"-h,required" help:"Host to connect to."`
	Port int    `arg:"-p" default:"65530" help:"Port to connect to."`

	TcpTimeout int `arg:"-t" default:"3000" help:"TCP timeout in seconds."`
}

// Command represents the command types
type Command int

const (
	CMD_NONE Command = iota
	CMD_GET_FORMAT
	CMD_START_PLAY
	CMD_HEARTBEAT
)

func (cmd Command) String() string {
	return [...]string{"CMD_NONE", "CMD_GET_FORMAT", "CMD_START_PLAY", "CMD_HEARTBEAT"}[cmd]
}

// TcpMessage represents a message sent over TCP
type TcpMessage struct {
	Command     Command
	AudioFormat *pb.AudioFormat
	ID          int
}

// Encode encodes the TcpMessage into a byte slice
func (tm *TcpMessage) Encode() []byte {
	buf := make([]byte, 4)

	// Command
	binary.LittleEndian.PutUint32(buf, uint32(tm.Command))

	return buf
}

// Decode decodes a byte slice into a TcpMessage
func (message *TcpMessage) Decode(data []byte) error {
	if len(data) < 4 {
		return fmt.Errorf("data too short to decode command")
	}

	// Command
	message.Command = Command(binary.LittleEndian.Uint32(data[:4]))
	data = data[4:]

	// AudioFormat if CMD_GET_FORMAT
	if message.Command == CMD_GET_FORMAT {
		if len(data) < 4 {
			return fmt.Errorf("data too short to decode audio format")
		}

		bufSize := int32(binary.LittleEndian.Uint32(data[:4]))
		data = data[4:]

		if len(data) < int(bufSize) {
			return fmt.Errorf("data too short to decode audio format")
		}

		message.AudioFormat = &pb.AudioFormat{}
		if err := proto.Unmarshal(data[:bufSize], message.AudioFormat); err != nil {
			return fmt.Errorf("failed to unmarshal audio format: %v", err)
		}

		log.Printf("Decoded audio format: %v", message.AudioFormat)
	}

	// ID if CMD_START_PLAY
	if message.Command == CMD_START_PLAY {
		if len(data) < 4 { // 1 int for the ID
			return fmt.Errorf("data too short to decode ID")
		}

		message.ID = int(binary.LittleEndian.Uint32(data[:4]))

		log.Printf("Decoded playback ID: %d", message.ID)
		log.Printf("Decoded playback ID: %d", message.ID)
		log.Printf("Decoded playback ID: %d", message.ID)
		log.Printf("Decoded playback ID: %d", message.ID)
	}

	return nil
}

func sendHeartbeat(conn *net.Conn) {
	ticker := time.NewTicker(3 * time.Second)
	defer ticker.Stop()

	for {
		<-ticker.C
		msg := &TcpMessage{
			Command: CMD_HEARTBEAT,
		}
		(*conn).Write(msg.Encode())
		log.Println("Sent heartbeat")
	}
}

func handleIncomingTCPData(conn *net.Conn) {
	buf := make([]byte, 1024)

	for {
		length, err := (*conn).Read(buf)
		if err != nil {
			if err.Error() == "EOF" {
				log.Println("Connection closed by server.")
				break
			}

			log.Println("Error reading:", err)
			continue
		}

		msg := &TcpMessage{}
		if err := msg.Decode(buf[:length]); err != nil {
			log.Println("Error decoding:", err)
			continue
		}

		if msg.Command == CMD_HEARTBEAT {
			log.Println("Received heartbeat")
		} else {
			log.Printf("Received message: %v", msg)
		}
	}
}

func handleIncomingUDPData(conn *net.Conn) {
	buf := make([]byte, 1024)

	for {
		n, err := (*conn).Read(buf)
		if err != nil {
			log.Printf("Error receiving UDP data: %v", err)
			continue
		}

		data := buf[:n]
		floatData := decodePCM(data)
		// Process floatData as needed
		log.Printf("Received audio data: %v", floatData)
	}
}

func decodePCM(data []byte) []float32 {
	buf := bytes.NewReader(data)
	floatData := make([]float32, len(data)/4)
	err := binary.Read(buf, binary.LittleEndian, floatData)
	if err != nil {
		log.Fatalf("binary.Read failed: %v", err)
	}
	return floatData
}

func main() {
	arg.MustParse(&args)

	log.Println("Host:", args.Host)
	log.Println("Port:", args.Port)

	tcpConn, err := net.Dial("tcp", fmt.Sprintf("%s:%d", args.Host, args.Port))
	if err != nil {
		panic(err)
	}
	defer tcpConn.Close()

	tcpConn.SetReadDeadline(time.Now().Add(time.Duration(args.TcpTimeout) * time.Second))

	log.Println("Connected to server.")

	// Send get format
	tcpConn.Write((&TcpMessage{Command: CMD_GET_FORMAT}).Encode())

	buf := make([]byte, 1024)

	if _, err := tcpConn.Read(buf); err != nil {
		panic(err)
	}

	msg := &TcpMessage{}
	if err := msg.Decode(buf); err != nil {
		panic(err)
	}

	if msg.Command != CMD_GET_FORMAT {
		panic("Expected CMD_GET_FORMAT")
	}

	audioFormat = msg.AudioFormat

	log.Println("Audio format:", audioFormat)

	// Send start play
	tcpConn.Write((&TcpMessage{Command: CMD_START_PLAY}).Encode())

	buf = make([]byte, 1024)
	if _, err := tcpConn.Read(buf); err != nil {
		panic(err)
	}

	msg = &TcpMessage{}
	if err := msg.Decode(buf); err != nil {
		panic(err)
	}

	if msg.Command != CMD_START_PLAY {
		panic("Expected CMD_START_PLAY")
	}

	log.Println("ID", msg)

	// start UDP listener
	udpConn, err := net.Dial("udp", fmt.Sprintf("%s:%d", args.Host, args.Port))
	if err != nil {
		panic(err)
	}

	idBuf := make([]byte, 4)
	binary.LittleEndian.PutUint32(idBuf, uint32(msg.ID))
	udpConn.Write(idBuf)

	go handleIncomingUDPData(&udpConn)

	go sendHeartbeat(&tcpConn)

	go handleIncomingTCPData(&tcpConn)

	select {}
}
