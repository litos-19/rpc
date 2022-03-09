class Config(object):
    CONTROLLER_DT = 0.001
    N_SUBSTEP = 1

    INITIAL_BASE_JOINT_POS = [0, 0, 0.95 - 0.21]
    # INITIAL_BASE_JOINT_POS = [0, 0, 1.5 - 0.757]
    INITIAL_BASE_JOINT_QUAT = [0, 0, 0, 1]

    PRINT_ROBOT_INFO = False
