# UI Screen Map - aRacer X Tune 3.7.2 to GODSPEED

Reference build: aRacer X Tune 3.7.2 (decompiled `Q1/c.java`, `Q1/f.java`, `mobile_navigation.xml`).
Legend: `VERIFIED STATIC` = shipped UI and path inspected; `PARTIAL` = UI or local implementation exists, protocol/behaviour incomplete; `UNKNOWN` = not traced; `NOT IMPLEMENTED` = no GODSPEED screen yet.

No row is `VERIFIED ECU`. Live values remain simulator/TCP only. Calibration, service, DTC clear, activation, and firmware writes are intentionally disabled.

## App destinations

| aRacer screen / route | Purpose | GODSPEED destination | Status |
|---|---|---|---|
| `nav_simple_monitor` / `Fragment_DB_Simple` | Simple dashboard | Monitor > Simple | VERIFIED STATIC |
| `nav_race_monitor` / `Fragment_DB_Race` | Race dashboard | Monitor > Race | VERIFIED STATIC |
| `nav_cube_monitor` / `Fragment_DB_Cube` | Cube dashboard | Monitor > Cube | VERIFIED STATIC |
| `gauge/scope/ScopeView` | Strip chart | Log View trace (limited) | PARTIAL |
| `RecordPlayer_Gauge` | Recorded-run replay | Log View local capture | PARTIAL |
| `Fragment_PowerTest` | Acceleration runs | Power Test | PARTIAL |
| `Activity_PowerCurveReport` | Power curve report | Runs list only | PARTIAL |
| `Activity_AccTestReport` | Acceleration report | Runs list only | PARTIAL |
| `calselection/Fragment_CalItems` | Calibration browser | Calibration > item list | VERIFIED STATIC |
| `Fragment_D1` / `Fragment_D1_NB_CL` | 1D scalar calibration | Calibration > slider editor | PARTIAL (local preview) |
| `Fragment_D1_AutoWarmup` | Auto warm-up | Catalog group only | PARTIAL |
| `Fragment_D1x4` / `Fragment_D1x5` | Multi-column calibration | Not implemented | NOT IMPLEMENTED |
| `Fragment_D3` / `Fragment_D3_InjAngle` | 3D table calibration | Calibration > 16x16 heatmap | PARTIAL (local preview) |
| `Fragment_AT` / `pro_at/Fragment_ProAT` | AT calibration | Not implemented | NOT IMPLEMENTED |
| `navfragment/calibration/ecvt/at/*` | eCVT AT | Not implemented | NOT IMPLEMENTED |
| `navfragment/calibration/ecvt/mt/*` | eCVT MT | Not implemented | NOT IMPLEMENTED |
| `Fragment_Matrix` | Fuel/ignition matrix | D3 heatmap only | PARTIAL |
| `Fragment_QuickShifter` / `Fragment_QS_Wizard` | Quick shifter | Catalog group only | PARTIAL |
| `Fragment_BackFire` | Backfire | Catalog group only | PARTIAL |
| `Fragment_eBoost[ _X2_Ecu]` | eBoost | Not implemented | NOT IMPLEMENTED |
| `Fragment_EXUP` | EXUP | Not implemented | NOT IMPLEMENTED |
| `Fragment_IdleAdj` / `IdleRpm_Adjuster` | Idle adjust | Catalog group only | PARTIAL |
| `Fragment_InjectorCal` | Injector calibration | Catalog group only | PARTIAL |
| `Fragment_TPSLearn` | TPS learn | Service action disabled | UNKNOWN |
| `Fragment_Serivice` | Service reset | Service action disabled | UNKNOWN |
| `Fragment_RButton` / `RButtonGridView` | R-button config | Not implemented | NOT IMPLEMENTED |
| `Fragment_RGB_Light` | RGB accessory | Not implemented | NOT IMPLEMENTED |
| `Fragment_WindShield` | Windshield accessory | Not implemented | NOT IMPLEMENTED |
| `sxs/Fragment_SxS_BoostCtrl` | SxS boost | Not implemented | NOT IMPLEMENTED |
| `sxs/Fragment_SxS_D3Swap` | SxS D3 swap | Not implemented | NOT IMPLEMENTED |
| `Fragment_MalfCode` | DTC list/clear | Malfunction Codes | UNKNOWN (read/clear frames not traced) |
| `Fragment_EcuState` | ECU state | ECU State | PARTIAL |
| `Fragment_SysInformation` | Identity/version | ECU State | PARTIAL |
| `MapStore_Fragment` | Map store | Map Store (local snapshots) | PARTIAL |
| `ImportFile_Activity` | Map import | Calibration JSON import only | PARTIAL |
| `LogFileList_Fragment` / `iView_Fragment` | Log list/viewer | Log View | PARTIAL |
| `Fragment_Setting` | Settings/units/logging | Settings | PARTIAL |
| `Fragment_EcuActivation` / `Fragment_EcuAct` | Activation/billing | Upgrade | UNKNOWN (no bypass) |
| `Activity_EcuTypeSwitch` | ECU model switch | Not implemented | NOT IMPLEMENTED |
| `setpages/Select_ECU_Fragment` | ECU selection | Not implemented | NOT IMPLEMENTED |
| `qrcode/ScanOverlayView` | QR scan | Not implemented | NOT IMPLEMENTED |
| `Opening_Activity` / `MainActivity` | Wi-Fi connect / launch | Settings (TCP host/port) | PARTIAL |
| `Fragment_SysInformation` upgrade card | Firmware/accessory upgrade | Disabled in Upgrade | UNKNOWN |
| Remote config (`n1.b.k`) | Startup templates | Not implemented | NOT IMPLEMENTED |

## New GODSPEED-only destinations

| Destination | Purpose | Status |
|---|---|---|
| Navigation | GPS route guidance | PARTIAL (device GPS, no routing engine yet) |
| Monitor preview toggle | Synthetic layout data, clearly labelled | Implemented, local only |

## Drawer mapping

| aRacer drawer entry (`Q1/f.java`) | GODSPEED drawer |
|---|---|
| 0 monitor cycle (`LITE` / `PRO` / `QUICK`) | Simple / Race / Cube |
| 1 Calibration | Calibration |
| 2 PowerTest | Power Test |
| 3 MalfCode | Malfunction Codes |
| 4 LogView | Log View |
| 5 Maps | Map Store |
| 6 QuickBurn | Not represented (no write path) |
| 7 Setting | Settings |
| 8 Purchase | Upgrade |
| 9 QR Scan | Not implemented |
| n/a | Navigation |
| n/a | ECU State |

## Capability boundaries

- Calibration edits persist to `SharedPreferences` only. `SEND TO ECU` is disabled.
- DTC clear, service resets, TPS learn, activation, firmware upgrade, and quick-burn have no enabled control.
- Live telemetry requires the TCP link; preview mode is always labelled synthetic.
- The catalog source is `ECU_PARAMETER_REGISTRY.json`; 1,493 items are loaded from `assets/calibration_catalog.json`.
