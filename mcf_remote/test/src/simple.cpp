/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/Mcf.h"
#include "mcf_core/TimeService.h"
#include "mcf_remote/RemoteService.h"

int main() {
  mcf::ValueStore vs;
  mcf::remote::RemoteReceiver rr(5555, vs);
}
