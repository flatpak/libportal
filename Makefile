# Build libportal
lib:
	meson build/lib; \
	cd build/lib; \
	ninja; \
	cd -; \

# Build and run the libportal test application
test:
	meson build/test -Dbuild-portal-test=true; \
	cd build/test; \
	ninja; \
	cd -; \
	chmod +x ./build/test/portal-test/portal-test; \
	echo "libportal: Just a test file" > $(HOME)/.local/share/test.txt; \
	./build/test/portal-test/portal-test; \

# Build libportal documentation
doc:
	meson build/doc -Dgtk_doc=true; \
	cd build/doc; \
	ninja; \
	cd -; \

# Get build and run-time system requirements
requirements:
	sudo apt install \
		python3 \
		python3-pip \
		python3-setuptools \
		python3-wheel \
		ninja-build \
		libgtk2.0-dev \
		libgtk-3-dev \
		gtk-doc-tools \
		libgstreamer-plugins-base1.0-dev; \

	pip3 install meson; \

.PHONY: lib test doc requirements
