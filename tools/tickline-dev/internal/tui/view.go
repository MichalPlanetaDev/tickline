package tui

import (
	"fmt"
	"strings"
	"time"

	"charm.land/lipgloss/v2"
)

type layoutMode int

const (
	layoutNarrow layoutMode = iota
	layoutMedium
	layoutWide
)

var (
	colorBackground = lipgloss.Color("#0B0F14")
	colorSurface    = lipgloss.Color("#111821")
	colorElevated   = lipgloss.Color("#18212D")
	colorBorder     = lipgloss.Color("#2B394A")
	colorPrimary    = lipgloss.Color("#E6EDF3")
	colorMuted      = lipgloss.Color("#8290A3")
	colorAccent     = lipgloss.Color("#78A9FF")
	colorRunning    = lipgloss.Color("#61D6D6")
	colorSuccess    = lipgloss.Color("#66D9A7")
	colorWarning    = lipgloss.Color("#E7C66B")
	colorFailure    = lipgloss.Color("#FF7B86")
	colorDisabled   = lipgloss.Color("#586474")
)

var (
	baseStyle = lipgloss.NewStyle().
			Foreground(colorPrimary).
			Background(colorBackground)

	headerStyle = lipgloss.NewStyle().
			Foreground(colorPrimary).
			Background(colorElevated).
			Bold(true).
			Padding(0, 1)

	panelStyle = lipgloss.NewStyle().
			Foreground(colorPrimary).
			Background(colorSurface).
			BorderStyle(lipgloss.RoundedBorder()).
			BorderForeground(colorBorder).
			Padding(0, 1)

	panelTitleStyle = lipgloss.NewStyle().
			Foreground(colorAccent).
			Bold(true)

	mutedStyle = lipgloss.NewStyle().
			Foreground(colorMuted)

	selectedStyle = lipgloss.NewStyle().
			Foreground(colorPrimary).
			Background(colorElevated).
			Bold(true)

	footerStyle = lipgloss.NewStyle().
			Foreground(colorMuted).
			Background(colorElevated).
			Padding(0, 1)

	errorStyle = lipgloss.NewStyle().
			Foreground(colorFailure).
			Bold(true)
)

func (model Model) render() string {
	width := max(model.width, 1)
	height := max(model.height, 1)

	if width < 36 || height < 12 {
		return model.renderMinimal(width)
	}

	header := model.renderHeader(width)
	footer := model.renderFooter(width)

	bodyHeight := max(
		height-lipgloss.Height(header)-lipgloss.Height(footer),
		6,
	)

	var body string

	switch chooseLayout(width) {
	case layoutWide:
		body = model.renderWide(
			width,
			bodyHeight,
		)

	case layoutMedium:
		body = model.renderMedium(
			width,
			bodyHeight,
		)

	default:
		body = model.renderNarrow(
			width,
			bodyHeight,
		)
	}

	content := lipgloss.JoinVertical(
		lipgloss.Left,
		header,
		body,
		footer,
	)

	return baseStyle.
		Width(width).
		Render(content)
}

func chooseLayout(width int) layoutMode {
	switch {
	case width >= 110:
		return layoutWide

	case width >= 72:
		return layoutMedium

	default:
		return layoutNarrow
	}
}

func (model Model) renderHeader(
	width int,
) string {
	runID := model.runID
	if runID == "" {
		runID = "initializing"
	}

	left := "TICKLINE  /  LOCAL VERIFICATION"
	right := "RUN  " + runID

	available := max(
		width-lipgloss.Width(left)-lipgloss.Width(right)-4,
		1,
	)

	content := left +
		strings.Repeat(" ", available) +
		right

	return headerStyle.
		Width(width).
		MaxWidth(width).
		Render(content)
}

func (model Model) renderFooter(
	width int,
) string {
	left := model.footerControls()
	right := model.footerStatus()

	available := max(
		width-lipgloss.Width(left)-lipgloss.Width(right)-4,
		1,
	)

	content := left +
		strings.Repeat(" ", available) +
		right

	return footerStyle.
		Width(width).
		MaxWidth(width).
		Render(content)
}

func (model Model) footerControls() string {
	switch {
	case model.completed:
		return "j/k navigate   g/G first/last   enter/q quit"

	case model.cancelling:
		return "cancellation requested"

	default:
		return "j/k navigate   g/G first/last   q/ctrl+c cancel"
	}
}

func (model Model) footerStatus() string {
	switch {
	case model.cancelling && !model.completed:
		return errorStyle.Render("CANCELLING")

	case model.completed && model.runError != nil:
		return errorStyle.Render("INTERNAL ERROR")

	case model.completed:
		return statusStyle(
			statusFromRunner(model.result.Status),
		).Render(
			strings.ToUpper(
				string(model.result.Status),
			),
		)

	default:
		completed, total := model.progress()

		return fmt.Sprintf(
			"%d/%d COMPLETE",
			completed,
			total,
		)
	}
}

func (model Model) renderWide(
	width int,
	height int,
) string {
	leftWidth := min(
		max(width/3, 34),
		46,
	)

	gap := 1
	rightWidth := max(
		width-leftWidth-gap,
		24,
	)

	taskPanel := model.renderTaskPanel(
		leftWidth,
		height,
	)

	logPanel := model.renderLogPanel(
		rightWidth,
		height,
	)

	return lipgloss.JoinHorizontal(
		lipgloss.Top,
		taskPanel,
		strings.Repeat(" ", gap),
		logPanel,
	)
}

func (model Model) renderMedium(
	width int,
	height int,
) string {
	taskHeight := min(
		max(len(model.stages)+3, 7),
		max(height/2, 7),
	)

	logHeight := max(
		height-taskHeight,
		6,
	)

	taskPanel := model.renderTaskPanel(
		width,
		taskHeight,
	)

	logPanel := model.renderLogPanel(
		width,
		logHeight,
	)

	return lipgloss.JoinVertical(
		lipgloss.Left,
		taskPanel,
		logPanel,
	)
}

func (model Model) renderNarrow(
	width int,
	height int,
) string {
	taskHeight := min(
		max(len(model.stages)+3, 7),
		max(height/2, 7),
	)

	logHeight := max(
		height-taskHeight,
		5,
	)

	return lipgloss.JoinVertical(
		lipgloss.Left,
		model.renderTaskPanel(
			width,
			taskHeight,
		),
		model.renderLogPanel(
			width,
			logHeight,
		),
	)
}

func (model Model) renderTaskPanel(
	width int,
	height int,
) string {
	contentWidth := max(width-4, 8)
	contentHeight := max(height-2, 3)

	lines := []string{
		panelTitleStyle.Render("CHECKS"),
	}

	if len(model.stages) == 0 {
		lines = append(
			lines,
			mutedStyle.Render("No stages selected."),
		)
	} else {
		for index, current := range model.stages {
			symbol := statusSymbol(current.Status)
			status := statusStyle(current.Status).
				Render(symbol)

			duration := ""
			if current.Duration > 0 {
				duration = formatTUIDuration(
					current.Duration,
				)
			}

			labelWidth := max(
				contentWidth-
					lipgloss.Width(symbol)-
					lipgloss.Width(duration)-
					5,
				4,
			)

			label := truncateText(
				current.Label,
				labelWidth,
			)

			spacing := max(
				contentWidth-
					lipgloss.Width(symbol)-
					lipgloss.Width(label)-
					lipgloss.Width(duration)-
					3,
				1,
			)

			row := fmt.Sprintf(
				"%s  %s%s%s",
				status,
				label,
				strings.Repeat(" ", spacing),
				mutedStyle.Render(duration),
			)

			if index == model.selected {
				row = selectedStyle.
					Width(contentWidth).
					MaxWidth(contentWidth).
					Render(row)
			}

			lines = append(lines, row)
		}
	}

	content := strings.Join(lines, "\n")

	return panelStyle.
		Width(width).
		Height(height).
		MaxWidth(width).
		MaxHeight(height).
		Render(
			tailVisibleLines(
				content,
				contentHeight,
			),
		)
}

func (model Model) renderLogPanel(
	width int,
	height int,
) string {
	contentWidth := max(width-4, 8)
	contentHeight := max(height-3, 2)

	stage, exists := model.selectedStage()

	title := "OUTPUT"

	if exists {
		title = "OUTPUT  /  " +
			strings.ToUpper(stage.ID)
	}

	lines := []string{
		panelTitleStyle.Render(title),
	}

	if !exists {
		lines = append(
			lines,
			mutedStyle.Render("No stage selected."),
		)
	} else {
		output := model.logs[stage.ID]

		if output == "" {
			output = "Waiting for stage output."
		}

		if model.logTruncated[stage.ID] {
			output = "[earlier output omitted]\n" +
				output
		}

		wrapped := lipgloss.Wrap(
			output,
			contentWidth,
			" ",
		)

		lines = append(
			lines,
			tailVisibleLines(
				wrapped,
				contentHeight,
			),
		)
	}

	content := strings.Join(lines, "\n")

	return panelStyle.
		Width(width).
		Height(height).
		MaxWidth(width).
		MaxHeight(height).
		Render(content)
}

func (model Model) renderMinimal(
	width int,
) string {
	stage, exists := model.selectedStage()

	stageText := "no stage"
	if exists {
		stageText = fmt.Sprintf(
			"%s [%s]",
			stage.Label,
			stage.Status,
		)
	}

	content := fmt.Sprintf(
		"Tickline\n%s\nq: cancel or quit",
		stageText,
	)

	return baseStyle.
		Width(width).
		MaxWidth(width).
		Render(content)
}

func (model Model) selectedStage() (
	stageState,
	bool,
) {
	if model.selected < 0 ||
		model.selected >= len(model.stages) {
		return stageState{}, false
	}

	return model.stages[model.selected], true
}

func (model Model) progress() (
	int,
	int,
) {
	completed := 0

	for _, current := range model.stages {
		switch current.Status {
		case stagePassed,
			stageFailed,
			stageSkipped,
			stageCancelled,
			stageInternalError:
			completed++
		}
	}

	return completed, len(model.stages)
}

func statusSymbol(status stageStatus) string {
	switch status {
	case stageRunning:
		return "◆"

	case stagePassed:
		return "✓"

	case stageFailed:
		return "✗"

	case stageSkipped:
		return "–"

	case stageCancelled:
		return "■"

	case stageInternalError:
		return "!"

	default:
		return "○"
	}
}

func statusStyle(
	status stageStatus,
) lipgloss.Style {
	switch status {
	case stageRunning:
		return lipgloss.NewStyle().
			Foreground(colorRunning).
			Bold(true)

	case stagePassed:
		return lipgloss.NewStyle().
			Foreground(colorSuccess).
			Bold(true)

	case stageFailed:
		return lipgloss.NewStyle().
			Foreground(colorFailure).
			Bold(true)

	case stageSkipped:
		return lipgloss.NewStyle().
			Foreground(colorDisabled)

	case stageCancelled:
		return lipgloss.NewStyle().
			Foreground(colorWarning).
			Bold(true)

	case stageInternalError:
		return lipgloss.NewStyle().
			Foreground(colorFailure).
			Bold(true)

	default:
		return lipgloss.NewStyle().
			Foreground(colorMuted)
	}
}

func tailVisibleLines(
	content string,
	maximum int,
) string {
	if maximum <= 0 {
		return ""
	}

	lines := strings.Split(content, "\n")

	if len(lines) <= maximum {
		return content
	}

	return strings.Join(
		lines[len(lines)-maximum:],
		"\n",
	)
}

func truncateText(
	value string,
	width int,
) string {
	if width <= 0 {
		return ""
	}

	if lipgloss.Width(value) <= width {
		return value
	}

	if width == 1 {
		return "…"
	}

	runes := []rune(value)

	for len(runes) != 0 &&
		lipgloss.Width(string(runes)+"…") > width {
		runes = runes[:len(runes)-1]
	}

	return string(runes) + "…"
}

func formatTUIDuration(
	duration time.Duration,
) string {
	if duration < time.Millisecond {
		return duration.Round(
			time.Microsecond,
		).String()
	}

	return duration.Round(
		time.Millisecond,
	).String()
}
