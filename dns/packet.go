package dns

import (
	"errors"
	"fmt"
)

var (
	ErrNoQuestions = errors.New("malformed DNS packet: zero questions in query")
)

// Packet represents a parsed DNS query packet.
type Packet struct {
	Header    Header
	Questions []Question
}

// ParsePacket parses raw UDP packet bytes into a structured DNS Packet.
func ParsePacket(data []byte) (*Packet, error) {
	header, err := ParseHeader(data)
	if err != nil {
		return nil, fmt.Errorf("failed to parse DNS header: %w", err)
	}

	offset := HeaderLen
	questions := make([]Question, 0, header.QDCount)

	for i := 0; i < int(header.QDCount); i++ {
		q, newOffset, err := ParseQuestion(data, offset)
		if err != nil {
			return nil, fmt.Errorf("failed to parse question %d: %w", i+1, err)
		}
		questions = append(questions, q)
		offset = newOffset
	}

	return &Packet{
		Header:    header,
		Questions: questions,
	}, nil
}
