MakefileSHELL=/bin/bash
CWD=$(shell pwd)
CMD ?= bash
DEV ?=y
ROOT_DIR ?= /pyshield
IMAGE_NAME ?= noaa-gfdl/pyshield

NUM_RANKS ?=6
MPIRUN_ARGS ?=--oversubscribe --mca btl_vader_single_copy_mechanism none
MPIRUN_CALL ?=mpirun -np $(NUM_RANKS) $(MPIRUN_ARGS)

TEST_DATA_LOC ?=test_data/
TEST_DATA_VERSION ?=8.1.3
TEST_DATA_HOST ?= https://portal.nccs.nasa.gov/datashare/astg/smt/pace-regression-data/
TEST_TYPE ?= baroclinic
TEST_RESOLUTION ?= c12
TEST_CONFIG ?= $(TEST_RESOLUTION)_$(NUM_RANKS)ranks
TEST_CASE ?=$(TEST_CONFIG)_$(TEST_TYPE)
TEST_ARGS ?=-vs

PORT ?=8888
RUN_FLAGS ?=--rm
RUN_FLAGS += -e FV3_DACEMODE=$(FV3_DACEMODE)
APP_NAME ?=pySHiELD_dev
SAVEPOINT_SETUP=pip list
THRESH_ARGS=--threshold_overrides_file=$(ROOT_DIR)/tests/savepoint/translate/overrides/$(TEST_TYPE).yaml

BUILD_FLAGS ?=
VOLUMES ?=

ifeq ($(DEV), y)
	VOLUMES += -v $(CWD):/pyshield
endif

TEST_DATA_TARFILE = $(TEST_DATA_VERSION)_$(TEST_CONFIG)_$(TEST_TYPE).physics.tar.gz

CONTAINER_CMD?=docker run $(RUN_FLAGS) $(VOLUMES) $(IMAGE_NAME)

.PHONY: lint build build_explicit clean enter dev notebook get_test_data savepoint_tests savepoint_tests_mpi test_dperiodic test_all

lint:
	pre-commit run --all-files

build:
	DOCKER_BUILDKIT=1 docker build \
		$(BUILD_FLAGS) \
		-f $(CWD)/Dockerfile \
		-t $(IMAGE_NAME) \
		.

build_explicit:
	PROGRESS_NO_TRUNC=1 docker build \
		--progress plain \
		--no-cache \
		$(BUILD_FLAGS) \
		-f $(CWD)/Dockerfile \
		-t $(IMAGE_NAME) \
		.

clean:
	if [ -n "$(docker images -q $(IMAGE_NAME))" ]; then \
		docker image rm $(IMAGE_NAME); \
	fi

	if [ -d .gt_cache* ]; then \
		rm -r .gt_cache*; \
	fi


enter:
	docker run --rm -it \
		$(VOLUMES) \
		-p=$(PORT):$(PORT) \
		--name="$(APP_NAME)" \
		$(IMAGE_NAME) $(CMD)

dev:
	DEV=y $(MAKE) enter

notebook:
	CMD="jupyter notebook --ip 0.0.0.0 --no-browser --allow-root --notebook-dir=$(ROOT_DIR)/examples/notebook" \
	DEV=y \
	$(MAKE) enter

get_test_data:
	if [ ! -d $(TEST_DATA_LOC) ]; then \
		mkdir $(TEST_DATA_LOC); \
	fi

	if [ ! -f "$(TEST_DATA_LOC)$(TEST_DATA_VERSION)/$(TEST_CASE)/physics/input.nml" ] ; then \
		wget $(TEST_DATA_HOST)$(TEST_DATA_TARFILE) ; \
		tar -xzvf $(TEST_DATA_TARFILE) ; \
		mv $(TEST_DATA_VERSION) $(TEST_DATA_LOC) ; \
		rm $(TEST_DATA_TARFILE); \
	fi

savepoint_tests:
	$(MAKE) get_test_data
	$(CONTAINER_CMD) bash -c "$(SAVEPOINT_SETUP) && cd $(ROOT_DIR) && pytest --data_path=$(TEST_DATA_LOC)$(TEST_DATA_VERSION)/$(TEST_CASE)/physics $(TEST_ARGS) $(THRESH_ARGS) $(ROOT_DIR)/tests/savepoint"

savepoint_tests_mpi:
	$(MAKE) get_test_data
	$(CONTAINER_CMD) bash -c "$(SAVEPOINT_SETUP) && cd $(ROOT_DIR) && $(MPIRUN_CALL) python3 -m mpi4py -m pytest --maxfail=1 --data_path=$(TEST_DATA_LOC)$(TEST_DATA_VERSION)/$(TEST_CASE)/physics $(TEST_ARGS) $(THRESH_ARGS) -m parallel $(ROOT_DIR)/tests/savepoint"

test_all:
	$(MAKE) savepoint_tests
	$(MAKE) savepoint_tests_mpi
