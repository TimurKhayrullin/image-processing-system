# Top-level Makefile for all executables
SUBDIRS := lib image_generator feature_extractor data_logger utility/psql_export_csv
BUILD_DIR := build

# Default target
all: $(SUBDIRS)

# Build each executable via its own Makefile
$(SUBDIRS):
	@echo "Building $@..."
	@$(MAKE) -C $@

# Clean everything
clean:
	@for dir in $(SUBDIRS); do \
		echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done
	rm -rf $(BUILD_DIR)

.PHONY: all clean $(SUBDIRS)
