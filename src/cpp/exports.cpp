#include "marketdata_okx.h"
#include "trader_okx.h"

#include <kungfu/wingchun/extension.h>

KUNGFU_EXTENSION(okx) {
  KUNGFU_DEFINE_MD(kungfu::wingchun::okx::MarketDataOkx);
  KUNGFU_DEFINE_TD(kungfu::wingchun::okx::TraderOkx);
}
