Code modules in the `utility` folder contain code with the following characteristics:

- _zero_ platform-specific implementations
- _may_ call KORL APIs; highly likely, but not required
- _may_ also be utilized within the KORL platform layer itself

Because of these characteristics, the following statements must hold true:

- the APIs of these modules are _absent_ from KORL API (`korl-interface-platform.h`)
- if a code module (such as a KORL client) wishes to use one of these modules, they _must_ include the implementation in their own translation unit
