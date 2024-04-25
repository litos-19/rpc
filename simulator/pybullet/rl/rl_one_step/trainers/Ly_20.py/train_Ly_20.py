import os
import sys
import numpy as np
import datetime
import time
from math import pi

import gymnasium as gym

from stable_baselines3 import PPO
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.vec_env import DummyVecEnv, SubprocVecEnv, VecNormalize
from stable_baselines3.common.callbacks import CallbackList, CheckpointCallback, EvalCallback

cwd = os.getcwd()
sys.path.append(cwd)
sys.path.append(cwd + "/build/lib")
from simulator.pybullet.rl.rl_one_step.envs.Ly_20 import DracoEnvOneStepMpcLy_20

from config.draco.pybullet_simulation import Config

model_dir = cwd + "/rl_model/Ly_20/PPO"
env_dir = cwd + "/rl_env/Ly_20/PPO"
#import tracemalloc
import argparse

new_model = True

if __name__ == "__main__":
    if not new_model:
        parser = argparse.ArgumentParser(description='Training script for your RL model')
        parser.add_argument('--timesteps', type=str, help='FileToLoad')  # Default value is set as an example
        args = parser.parse_args()
        bash_timesteps = int(args.timesteps)

    n_steps_ = 256 #512
    batch_size_ = 64
    learning_rate_ = 0.0003
    mpc_freq = 0
    sim_dt = 0.00175
    reduced_obs_size = False

    render = False
    env = DracoEnvOneStepMpcLy_20(mpc_freq, sim_dt, reduced_obs_size=reduced_obs_size, render = render)

    monitor_env = Monitor(env)
    vec_env = DummyVecEnv([lambda: monitor_env])

    #MODEL EVALUATION
    eval_env = DracoEnvOneStepMpcLy_20(mpc_freq, sim_dt,reduced_obs_size=reduced_obs_size, render = render)
    eval_monitor_env = Monitor(eval_env)
    eval_vec_env = DummyVecEnv([lambda: eval_monitor_env])
    norm_eval_env = VecNormalize(eval_vec_env,norm_obs = True, norm_reward = False, clip_obs = 60, gamma = 0.99)
    eval_callback = EvalCallback(norm_eval_env, eval_freq=5000, deterministic=True, render = False, )

    if reduced_obs_size:
        str1 = 'redObs'
    else:
        str1 = 'fullObs'
    
    save_dir = str1 + f"Ly_20" 
    load_dir = str1 + f"Ly_20"
    load_path = os.path.join(model_dir, load_dir) 
    save_path = os.path.join(model_dir, save_dir)      
    ## train model
    if new_model:
        tensorboard_dir = cwd + "/rl_log/one_step/ppo/Ly_20/"

        norm_env = VecNormalize(vec_env, norm_obs = True, norm_reward = False, clip_obs = 60, gamma = 0.99)

        model = PPO("MlpPolicy", norm_env, verbose=1, 
                    n_steps = n_steps_, 
                    batch_size=batch_size_, 
                    tensorboard_log=tensorboard_dir, 
                    learning_rate=learning_rate_,
                    device='cpu') #policy_kwargs=dict(net_arch=[64,64, dict(vf=[], pi=[])]),
         
        startTime = time.time()
        TIMESTEPS = 10000
        CURR_TIMESTEP = 0
    else:
        startTime = time.time()
        CURR_TIMESTEP = bash_timesteps

        save_subdir = f"_TIME{CURR_TIMESTEP}"
        model_path = os.path.join(load_path, save_subdir)
        env_file = f"TIME{CURR_TIMESTEP}.pkl"
        save_env_path = os.path.join(load_path, env_file)
        norm_env = VecNormalize.load(save_env_path, vec_env)

        #learning_rate_ = 0.00003 #OLD LEARNING RATE 0.0003
        custom_objects = {'learning_rate': learning_rate_}
        model = PPO.load(model_path, env=norm_env, custom_objects=custom_objects)

        TIMESTEPS =10000


    while(True):
        try:
            model.learn(total_timesteps=TIMESTEPS, 
                        progress_bar=True, 
                        reset_num_timesteps=False, 
                        tb_log_name=save_dir, 
                        callback=eval_callback)
            endTime = time.time()
            print("Model train time: "+str(datetime.timedelta(seconds=endTime-startTime)))
            ## save the model
            CURR_TIMESTEP += TIMESTEPS
            save_subdir = f"_TIME{CURR_TIMESTEP}"
            model_path = save_path + '/' + save_subdir
            env_file = f"TIME{CURR_TIMESTEP}.pkl"
            save_env_path = os.path.join(save_path, env_file)
            print(save_path)
            model.save(model_path)
            print(save_env_path)
            norm_env.save(save_env_path)

        except Exception as e:
            print(f"An error occurred during training: {e}")
            endTime = time.time()
            #except ValueError:
            # close the progress bar
            #callbacks[1].on_training_end()
            #pbar_callback = ProgressBarCallback()
            #model = PPO.load(save_path, env = env)
