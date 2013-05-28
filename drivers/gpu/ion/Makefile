ccflags-y := -Ofast -ffast-math -fgcse-lm -fgcse-sm -fsched-spec-load -fforce-addr -fsingle-precision-constant -mcpu=cortex-a15 -mtune=cortex-a15 -marm -mfpu=neon-vfpv4 -ftree-vectorize -mvectorize-with-neon-quad
obj-$(CONFIG_ION) +=	ion.o ion_heap.o ion_system_heap.o ion_carveout_heap.o ion_iommu_heap.o ion_cp_heap.o
obj-$(CONFIG_CMA) += ion_cma_heap.o
obj-$(CONFIG_ION_TEGRA) += tegra/
obj-$(CONFIG_ION_MSM) += msm/
