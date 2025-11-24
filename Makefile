help:
	@echo "Available targets:"
	@echo "  help     - Show this help message"
	@echo "  build -an    - Static Build for android"
	@echo "  build -ios   - Static Build the ios"
	@echo "  ffi      - Generate dart bindings from c header"
	@echo "  jucer    - Generate native projects from jucer file"


jucer:
	sh setup_clean_projects.sh

ffi:
	cd juce_mix_player_package && dart run ffigen

build-an:
	sh build_android_juce_lib.sh

build-ios:
	sh build_ios_juce_lib.sh

clean-all:
	cd flutter_app && fvm flutter clean && fvm flutter pub get && cd ios && pod deintegrate && pod install