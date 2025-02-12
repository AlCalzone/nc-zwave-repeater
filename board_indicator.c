// A modification of protocol/z-wave/platform/SiliconLabs/AppsHw/src/common/board_indicator.c
// to work with our custom LEDs

#include "board_indicator.h"

static bool m_indicator_active_from_cc = false;

void Board_IndicateStatus(board_status_t status) {
	// TODO
	(void) status;
}

void Board_IndicatorInit(void) {
	Board_IndicateStatus(BOARD_STATUS_IDLE);
}

bool Board_IndicatorControl(uint32_t on_time_ms,
                            uint32_t off_time_ms,
                            uint32_t num_cycles,
                            bool called_from_indicator_cc)
{
	// TODO
	(void) on_time_ms;
	(void) off_time_ms;
	(void) num_cycles;
	(void) called_from_indicator_cc;
	// TODO: m_indicator_active_from_cc = called_from_indicator_cc;
	return true;
}

bool Board_IsIndicatorActive(void)
{
  return m_indicator_active_from_cc;
}
