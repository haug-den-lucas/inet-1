import filecmp
import logging
import matplotlib
import sewar

from inet.common import *
from inet.documentation.chart import *
from inet.test.task import *
from inet.test.statistical import *

logger = logging.getLogger(__name__)

class ChartTestTask(TestTask):
    def __init__(self, simulation_project, analysis_file_name, chart_name, name="chart test", **kwargs):
        super().__init__(name=name, **kwargs)
        self.simulation_project = simulation_project
        self.analysis_file_name = analysis_file_name
        self.chart_name = chart_name

    def get_parameters_string(self, **kwargs):
        return self.analysis_file_name + ": " + self.chart_name

    def run_protected(self, keep_charts=False, output_stream=sys.stdout, **kwargs):
        workspace = omnetpp.scave.analysis.Workspace(get_workspace_path("."), [])
        analysis = omnetpp.scave.analysis.load_anf_file(self.simulation_project.get_full_path(self.analysis_file_name))
        for chart in analysis.collect_charts():
            if chart.name == self.chart_name:
                image_export_filename = chart.properties["image_export_filename"]
                folder = os.path.dirname(self.simulation_project.get_full_path(self.analysis_file_name))
                file_name = analysis.export_image(chart, folder, workspace, format="png", dpi=150, target_folder="doc/media", filename=image_export_filename + "_new")
                new_file_name = os.path.join(folder, file_name)
                old_file_name = os.path.join(folder, re.sub("_new", "", file_name))
                if image_export_filename is None or image_export_filename == "":
                    return self.task_result_class(self, result="SKIP", expected_result="SKIP", reason="Chart file name is not specified")
                elif os.path.isfile(old_file_name):
                    new_image = matplotlib.image.imread(new_file_name)
                    old_image = matplotlib.image.imread(old_file_name)
                    metric = sewar.rmse(old_image, new_image)
                    if not keep_charts:
                        os.remove(new_file_name)
                    result = "PASS" if metric == 0 else "FAIL"
                    reason = "Metric: " + str(metric) if result == "FAIL" else None
                    return self.task_result_class(self, result=result, reason=reason)
                else:
                    return self.task_result_class(self, result="FAIL", reason="Chart not found")
        return self.task_result_class(self, result="ERROR", reason="Chart not found")

class MultipleChartTestTasks(MultipleTestTasks):
    def __init__(self, tasks, multiple_simulation_tasks, name="chart test", multiple_task_results_class=MultipleTestTaskResults, **kwargs):
        super().__init__(tasks, name=name, multiple_task_results_class=multiple_task_results_class, **kwargs)
        self.multiple_simulation_tasks = multiple_simulation_tasks

    def run_protected(self, **kwargs):
        multiple_simulation_task_results = self.multiple_simulation_tasks.run_protected(output_stream=io.StringIO(), concurrent=False, **kwargs)
        if multiple_simulation_task_results.result != "DONE":
            return self.task_result_class(self, result=simulation_task_result.result, reason=simulation_task_result.reason)
        else:
            return super().run_protected(**kwargs)

def get_chart_test_tasks(simulation_project=default_project, run_simulations=True, filter=None, working_directory_filter=None, pool_class=multiprocessing.Pool, **kwargs):
    test_tasks = []
    simulation_tasks = []
    for analysis_file_name in get_analysis_files(simulation_project=simulation_project, filter=filter or working_directory_filter, **kwargs):
        analysis = omnetpp.scave.analysis.load_anf_file(simulation_project.get_full_path(analysis_file_name))
        for chart in analysis.collect_charts():
            folder = os.path.dirname(simulation_project.get_full_path(analysis_file_name))
            working_directory = os.path.relpath(folder, simulation_project.get_full_path("."))
            if run_simulations:
                multiple_simulation_tasks = get_simulation_tasks(simulation_project=simulation_project, working_directory_filter=working_directory, sim_time_limit=get_statistical_result_sim_time_limit, **kwargs)
                for simulation_task in multiple_simulation_tasks.tasks:
                    if not list(builtins.filter(lambda element: element.simulation_config == simulation_task.simulation_config and element._run == simulation_task._run, simulation_tasks)):
                        simulation_tasks.append(simulation_task)
            test_tasks.append(ChartTestTask(simulation_project, analysis_file_name, chart.name, task_result_class=TestTaskResult))
    return MultipleChartTestTasks(test_tasks, MultipleSimulationTasks(simulation_tasks, simulation_project=simulation_project, **kwargs), pool_class=pool_class, **kwargs)

def run_chart_tests(**kwargs):
    multiple_chart_test_tasks = get_chart_test_tasks(**kwargs)
    return multiple_chart_test_tasks.run(**kwargs)

class ChartUpdateTask(UpdateTask):
    def __init__(self, simulation_project, analysis_file_name, chart_name, **kwargs):
        super().__init__(**kwargs)
        self.simulation_project = simulation_project
        self.analysis_file_name = analysis_file_name
        self.chart_name = chart_name

    def get_parameters_string(self, **kwargs):
        return self.analysis_file_name + ": " + self.chart_name

    def run_protected(self, **kwargs):
        workspace = omnetpp.scave.analysis.Workspace(get_workspace_path("."), [])
        analysis = omnetpp.scave.analysis.load_anf_file(self.simulation_project.get_full_path(self.analysis_file_name))
        for chart in analysis.collect_charts():
            if chart.name == self.chart_name:
                image_export_filename = chart.properties["image_export_filename"]
                folder = os.path.dirname(self.simulation_project.get_full_path(self.analysis_file_name))
                file_name = analysis.export_image(chart, folder, workspace, format="png", dpi=150, target_folder="doc/media", filename=image_export_filename + "_new")
                new_file_name = os.path.join(folder, file_name)
                old_file_name = os.path.join(folder, re.sub("_new", "", file_name))
                if image_export_filename is None or image_export_filename == "":
                    return self.task_result_class(self, result="SKIP", expected_result="SKIP", reason="Chart file name is not specified")
                elif os.path.isfile(old_file_name):
                    same = filecmp.cmp(old_file_name, new_file_name, shallow=False)
                    if same:
                        os.remove(new_file_name)
                    else:
                        os.rename(new_file_name, old_file_name)
                    return self.task_result_class(self, result="KEEP" if same else "UPDATE")
                else:
                    os.rename(new_file_name, old_file_name)
                    return self.task_result_class(self, result="INSERT")
        return self.task_result_class(self, result="ERROR", reason="Chart not found")

def get_update_chart_tasks(simulation_project=default_project, filter=None, working_directory_filter=None, pool_class=multiprocessing.Pool, **kwargs):
    update_tasks = []
    for analysis_file_name in get_analysis_files(simulation_project=simulation_project, filter=filter or working_directory_filter, **kwargs):
        analysis = omnetpp.scave.analysis.load_anf_file(simulation_project.get_full_path(analysis_file_name))
        for chart in analysis.collect_charts():
            update_tasks.append(ChartUpdateTask(simulation_project, analysis_file_name, chart.name, task_result_class=UpdateTaskResult))
    return MultipleUpdateTasks(update_tasks, multiple_task_results_class=MultipleUpdateTaskResults, pool_class=pool_class, **kwargs)

def update_charts(simulation_project=default_project, pool_class=multiprocessing.Pool, **kwargs):
    multiple_update_chart_tasks = get_update_chart_tasks(**kwargs)
    return multiple_update_chart_tasks.run(**kwargs)