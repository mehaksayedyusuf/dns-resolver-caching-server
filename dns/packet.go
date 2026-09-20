package dns

import (
	"fmt"
)

// Packet represents a parsed DNS query or response packet.
type Packet struct {
	Header    Header
	Questions []Question
	Answers   []ResourceRecord
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

	answers := make([]ResourceRecord, 0, header.ANCount)
	for i := 0; i < int(header.ANCount); i++ {
		ans, newOffset, err := ParseAnswer(data, offset)
		if err != nil {
			return nil, fmt.Errorf("failed to parse answer %d: %w", i+1, err)
		}
		answers = append(answers, ans)
		offset = newOffset
	}

	return &Packet{
		Header:    header,
		Questions: questions,
		Answers:   answers,
	}, nil
}
