package tui

import (
	"strings"
	"unicode/utf8"
)

func sanitizeTerminalOutput(
	data []byte,
) string {
	if len(data) == 0 {
		return ""
	}

	output := make([]byte, 0, len(data))

	for index := 0; index < len(data); {
		current := data[index]

		switch {
		case current == 0x1b:
			index = consumeEscapeSequence(
				data,
				index,
			)

		case current == '\r':
			if index+1 < len(data) &&
				data[index+1] == '\n' {
				index++
			}

			output = append(output, '\n')
			index++

		case current == '\t':
			output = append(
				output,
				' ',
				' ',
				' ',
				' ',
			)

			index++

		case current == '\n':
			output = append(output, current)
			index++

		case current < 0x20 || current == 0x7f:
			index++

		default:
			output = append(output, current)
			index++
		}
	}

	result := string(output)

	if !utf8.ValidString(result) {
		result = strings.ToValidUTF8(
			result,
			"�",
		)
	}

	return result
}

func consumeEscapeSequence(
	data []byte,
	start int,
) int {
	if start+1 >= len(data) {
		return len(data)
	}

	switch data[start+1] {
	case '[':
		return consumeCSI(data, start+2)

	case ']', 'P', '^', '_':
		return consumeStringSequence(
			data,
			start+2,
		)

	default:
		return min(start+2, len(data))
	}
}

func consumeCSI(
	data []byte,
	start int,
) int {
	for index := start; index < len(data); index++ {
		if data[index] >= 0x40 &&
			data[index] <= 0x7e {
			return index + 1
		}
	}

	return len(data)
}

func consumeStringSequence(
	data []byte,
	start int,
) int {
	for index := start; index < len(data); index++ {
		if data[index] == 0x07 {
			return index + 1
		}

		if data[index] == 0x1b &&
			index+1 < len(data) &&
			data[index+1] == '\\' {
			return index + 2
		}
	}

	return len(data)
}
