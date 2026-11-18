# Schema Changes

Before changing the C++ document model or its serializers, read the Schema
Change Notes in `README.md`. Keep the shared robot-facing target independent of
Immer, LLNL units, and `wpilib-expression`; keep app-only behavior in the app
target. Follow the repository-wide schema-upgrade process when changing file
versions.