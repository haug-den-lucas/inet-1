import datetime
import functools
import logging
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import time

from inet.common import *
from inet.simulation.build import *
from inet.simulation.config import *
from inet.simulation.project import *
from inet.simulation.subprocess import *

logger = logging.getLogger(__name__)

class SimulationTask(Task):
    def __init__(self, simulation_config, run=0, mode="debug", user_interface="Cmdenv", sim_time_limit=None, cpu_time_limit=None, record_eventlog=None, record_pcap=None, name="simulation", **kwargs):
        super().__init__(name=name, **kwargs)
        assert run is not None
        self.simulation_config = simulation_config
        self.interactive = None # NOTE delayed to is_interactive()
        self._run = run
        self.mode = mode
        self.user_interface = user_interface
        self.sim_time_limit = sim_time_limit
        self.cpu_time_limit = cpu_time_limit
        self.record_eventlog = record_eventlog
        self.record_pcap = record_pcap

    # TODO replace this with something more efficient?
    def is_interactive(self):
        if self.interactive is None:
            simulation_config = self.simulation_config
            simulation_project = simulation_config.simulation_project
            executable = simulation_project.get_full_path("bin/inet")
            args = [executable, "-s", "-u", "Cmdenv", "-f", simulation_config.ini_file, "-c", simulation_config.config, "-r", "0", "--sim-time-limit", "0s"]
            env = os.environ.copy()
            env["INET_ROOT"] = simulation_project.get_full_path(".")
            subprocess_result = subprocess.run(args, cwd=simulation_project.get_full_path(simulation_config.working_directory), capture_output=True, env=env)
            stderr = subprocess_result.stderr.decode("utf-8")
            match = re.search(r"The simulation wanted to ask a question|The simulation attempted to prompt for user input", stderr)
            self.interactive = match is not None
        return self.interactive

    def get_parameters_string(self, **kwargs):
        working_directory = self.simulation_config.working_directory
        ini_file = self.simulation_config.ini_file
        config = self.simulation_config.config
        return working_directory + \
               (" -f " + ini_file if ini_file != "omnetpp.ini" else "") + \
               (" -c " + config if config != "General" else "") + \
               (" -r " + str(self._run) if self._run != 0 else "") + \
               (" for " + self.sim_time_limit if self.sim_time_limit else "")

    def get_sim_time_limit(self):
        return self.sim_time_limit(self.simulation_config, self._run) if callable(self.sim_time_limit) else self.sim_time_limit

    def get_cpu_time_limit(self):
        return self.cpu_time_limit(self.simulation_config, self._run) if callable(self.cpu_time_limit) else self.cpu_time_limit

    def run_protected(self, extra_args=[], simulation_runner=subprocess_simulation_runner, **kwargs):
        simulation_project = self.simulation_config.simulation_project
        working_directory = self.simulation_config.working_directory
        ini_file = self.simulation_config.ini_file
        config = self.simulation_config.config
        sim_time_limit_args = ["--sim-time-limit", self.get_sim_time_limit()] if self.sim_time_limit else []
        cpu_time_limit_args = ["--cpu-time-limit", self.get_cpu_time_limit()] if self.cpu_time_limit else []
        record_eventlog_args = ["--record-eventlog", "true"] if self.record_eventlog else []
        record_pcap_args = ["--**.numPcapRecorders=1", "--**.crcMode=\"computed\"", "--**.fcsMode=\"computed\""] if self.record_pcap else []
        executable = simulation_project.get_full_path("bin/inet")
        args = [executable, "--" + self.mode, "-s", "-u", self.user_interface, "-f", ini_file, "-c", config, "-r", str(self._run), *sim_time_limit_args, *cpu_time_limit_args, *record_eventlog_args, *record_pcap_args, *extra_args]
        env = os.environ.copy()
        env["INET_ROOT"] = simulation_project.get_full_path(".")
        logger.debug(args)
        subprocess_result = simulation_runner.run(self, args)
        if subprocess_result.returncode == -signal.SIGINT.value:
            return SimulationTaskResult(self, "CANCEL", subprocess_result, reason="Cancel by user")
        elif subprocess_result.returncode == 0:
            return SimulationTaskResult(self, "DONE", subprocess_result)
        else:
            return SimulationTaskResult(self, "ERROR", subprocess_result, reason="Non-zero exit code")

class MultipleSimulationTasks(MultipleTasks):
    def __init__(self, simulation_tasks, simulation_project=default_project, name="simulation", **kwargs):
        super().__init__(simulation_tasks, name=name, **kwargs)
        self.simulation_project = simulation_project

    def run(self, build=True, **kwargs):
        if build:
            build_project(simulation_project=self.simulation_project, **kwargs)
        return super().run(**kwargs)

class SimulationTaskResult(TaskResult):
    def __init__(self, simulation_task, result, subprocess_result, cancel=False, **kwargs):
        super().__init__(simulation_task, result, **kwargs)
        self.subprocess_result = subprocess_result
        if subprocess_result:
            stdout = self.subprocess_result.stdout.decode("utf-8")
            stderr = self.subprocess_result.stderr.decode("utf-8")
            match = re.search(r"<!> Simulation time limit reached -- at t=(.*), event #(\d+)", stdout)
            self.last_event_number = int(match.group(2)) if match else None
            self.last_simulation_time = match.group(1) if match else None
            self.elapsed_cpu_time = None # TODO
            match = re.search("<!> Error: (.*) -- in module (.*)", stderr)
            self.error_message = match.group(1).strip() if match else None
            self.error_module = match.group(2).strip() if match else None
            if self.error_message is None:
                match = re.search("<!> Error: (.*)", stderr)
                self.error_message = match.group(1).strip() if match else None
        else:
            self.last_event_number = None
            self.last_simulation_time = None
            self.error_message = None
            self.error_module = None

    def get_error_message(self, complete_error_message=True, **kwargs):
        error_message = self.error_message or "Error message not found"
        error_module = self.error_module or "Error module not found"
        return (error_message + " -- in module " + error_module if complete_error_message else error_message) if self.result == "ERROR" else ""

    def get_subprocess_result(self):
        return self.subprocess_result

def clean_simulation_results(simulation_config):
    logger.info("Cleaning simulation results, folder = " + simulation_config.working_directory)
    simulation_project = simulation_config.simulation_project
    path = os.path.join(simulation_project.get_full_path(simulation_config.working_directory), "results")
    if not re.search(".*/home/.*", path):
        raise Exception("Path is not in home")
    if os.path.exists(path):
        shutil.rmtree(path)

def clean_simulations_results(simulation_configs=None, **kwargs):
    if not simulation_configs:
        simulation_configs = get_simulation_configs(**kwargs)
    for simulation_config in simulation_configs:
        clean_simulation_results(simulation_config)

def get_simulation_tasks(simulation_project=None, simulation_configs=None, run=None, sim_time_limit=None, cpu_time_limit=None, concurrent=True, simulation_task_class=SimulationTask, multiple_simulation_tasks_class=MultipleSimulationTasks, **kwargs):
    if simulation_project is None:
        simulation_project = default_project
    if simulation_configs is None:
        simulation_configs = get_simulation_configs(simulation_project, concurrent=concurrent, **kwargs)
    simulation_tasks = []
    for simulation_config in simulation_configs:
        if run is not None:
            simulation_run_sim_time_limit = sim_time_limit(simulation_config, run) if callable(sim_time_limit) else sim_time_limit
            simulation_tasks.append(simulation_task_class(simulation_config, run, sim_time_limit=simulation_run_sim_time_limit, cpu_time_limit=cpu_time_limit, **kwargs))
        else:
            for generated_run in range(0, simulation_config.num_runs):
                simulation_run_sim_time_limit = sim_time_limit(simulation_config, generated_run) if callable(sim_time_limit) else sim_time_limit
                simulation_tasks.append(simulation_task_class(simulation_config, generated_run, sim_time_limit=simulation_run_sim_time_limit, cpu_time_limit=cpu_time_limit, **kwargs))
    return multiple_simulation_tasks_class(simulation_tasks, simulation_project=simulation_project, concurrent=concurrent, **kwargs)

def run_simulations(**kwargs):
    multiple_simulation_tasks = get_simulation_tasks(**kwargs)
    return multiple_simulation_tasks.run()

def run_simulation(simulation_project, working_directory, ini_file="omnetpp.ini", config="General", run=0, sim_time_limit=None, cpu_time_limit=None, **kwargs):
    simulation_config = SimulationConfig(simulation_project, working_directory, ini_file, config, 1, False, None)
    simulation_task = SimulationTask(simulation_config, run, sim_time_limit=sim_time_limit, cpu_time_limit=cpu_time_limit, **kwargs)
    return simulation_task.run(**kwargs)