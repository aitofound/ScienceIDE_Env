set(ENV{GIT_DIR} ${SRC_DIR}/.git)
set(ENV{GIT_WORK_TREE} ${SRC_DIR})
execute_process(COMMAND sh ${SRC_DIR}/gen_commit_string.sh .)
