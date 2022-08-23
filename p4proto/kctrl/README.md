<!--
/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
- -->

# P4OVS Kernel Control Interface (p4proto/kctrl)

P4OVS kernel control plane listens to kernel events using Switchlink library which provides
a netlink listener and uses Switch Abstraction Interfcae(SAI) layer to program the P4 compliant
target by interacting with a target agnostic frontend layer TDI (Table Driven Interface) through
Switchapi layer. p4proto/kctrl directory contains the code to manage kernel interface

## Table of Contents

- [Switchlink](/p4proto/kctrl/switchlink/README.md)
- [SwitchSAI](/p4proto/kctrl/switchsai/README.md)
- [Switchapi](/p4proto/kctrl/switchapi/README.md)
- [Linux Networking usecase](/p4proto/p4src/linux_networking/README_LINUX_NETWORKING.md)
- [Linux Networking with ECMP usecase](/p4proto/p4src/linux_networking/README_LINUX_NETWORKING_WITH_ECMP.md)
