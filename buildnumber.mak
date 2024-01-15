#BUILD_NUMBER_LFLAGS  = -Xlinker --defsym -Xlinker __BUILD_DATE="$$(sh date.sh)"
#BUILD_NUMBER_LFLAGS += -Xlinker --defsym -Xlinker __BUILD_NUMBER=$$(cat $(BUILD_NUMBER_FILE))

# Build number file.  Increment if any object file changes.
$(BUILD_NUMBER_FILE): $(O_FILES)
	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)
