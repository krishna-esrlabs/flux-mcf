""""
Copyright (c) 2024 Accenture
"""
import mcf_python_path.mcf_paths
from mcf import RemoteControl, ReplayParams, PlaybackModifier
import copy
import argparse

import PySimpleGUI as sg


DEFAULT_IP = '10.11.0.40'
DEFAULT_PORT = 6666


class LedIndicator(object):
    def __init__(self, key):
        self.key = key

    def create_led_indicator(self, radius=30):
        return sg.Graph(
            canvas_size=(radius, radius),
            graph_bottom_left=(-radius, -radius),
            graph_top_right=(radius, radius),
            pad=(0, 0),
            key=self.key)

    def set_led(self, window, color):
        graph = window[self.key]
        graph.erase()
        graph.draw_circle((0, 0), 12, fill_color=color, line_color=color)


class ReplayControlGui(object):
    def __init__(self, remote_control):
        # MCF parameters
        self.remote_control = remote_control

        # GUI parameters
        self.window = None
        self.window_name = 'Replay Control'
        self.window_size = (1500, 1000)
        self.is_playback_paused = False
        self.max_num_trigger_events = 5

        self.run_state_led = LedIndicator('-RUNSTATE-')

        # Get existing config from port
        self.replay_params = self.remote_control.get_replay_params()

        self._create_window()
        self._write_text_box_values()

    def _create_window(self):
        sg.theme('BluePurple')

        tree = self._create_topic_tree()
        sim_time_section, run_mode_section, event_section, param_section = self._create_sections()

        layout = [[
            sg.Column([[sg.Frame('Topic Tree', tree)]]),
            sg.Column([[sg.Frame('Simulation Time',
                                 sim_time_section)], [sg.VerticalSeparator()],
                       [sg.Frame('Run Mode',
                                 run_mode_section)], [sg.VerticalSeparator()],
                       [sg.Frame('Replay Event',
                                 event_section)], [sg.VerticalSeparator()],
                       [sg.Frame('Parameters', param_section)]])
        ]]

        if self.window is not None:
            self.window.close()

        self.window = sg.Window(
            title=self.window_name,
            layout=layout,
            auto_size_text=True,
            auto_size_buttons=True,
            size=self.window_size).Finalize()
        self._write_text_box_values()

    def _create_topic_tree(self):
        info = self.remote_control.get_info()
        treedata = sg.TreeData()

        for ci, comp in enumerate(info):
            root = ''
            treedata.Insert(
                parent=root, key=comp['name'], text=comp['name'], values=[''])

            for pi, port in enumerate(comp['ports']):
                treedata.Insert(
                    parent=comp['name'],
                    key=comp['name'] + str(ci) + str(pi),
                    text='#{}'.format(pi),
                    values=(port['topic']))

        tree = [[
            sg.Tree(
                data=treedata,
                headings=['Topic' + 75 * ' '],
                auto_size_columns=True,
                num_rows=100,
                max_col_width=1000,
                col0_width=30,
                key='-TREE-',
                justification='left',
                show_expanded=True,
                enable_events=True)
        ]]

        return tree

    def _create_sections(self):
        sim_time_section = [[sg.Text('Simulation Time (s): '),
                             sg.Text('0', key='-SIMTIMETEXT-', size=(20, 1))]]

        run_mode_section = \
            [[sg.Text('Run Mode:'), sg.Text(size=(5, 1)),
              sg.Column([[sg.Radio('Continuous', 'RADIO_RUNMODE', key='-CONTINUOUS-',
                       enable_events=True),
              sg.Radio('Single Steps', 'RADIO_RUNMODE', key='-SINGLESTEP-',
                       enable_events=True),
              sg.Radio('Step Time', 'RADIO_RUNMODE', key='-STEPTIME-',
                       enable_events=True)],
             [sg.Checkbox('Run without drops', key='-RUNWITHOUTDROPS-',
                          enable_events=True, default=True)]])]]

        event_section = \
            [[sg.Text('Event:'), sg.Text(size=(5, 1)),
              sg.Button('Pause', key='-PAUSE-'),
              sg.Button('Resume', key='-RESUME-'),
              sg.Button('Step Once', key='-STEPONCE-'),
              sg.Button('Exit'), self.run_state_led.create_led_indicator()]]

        trigger_names = self.replay_params.pipeline_end_trigger_names
        trigger_inputs = [[
            sg.Input(default_text=trigger_names[i] if i < len(trigger_names) else '',
                     key='-TRIGGEREVENT{}-'.format(i)),
        ] for i in range(self.max_num_trigger_events)]

        wait_events = self.replay_params.wait_input_event_name
        wait_topics = self.replay_params.wait_input_event_topic
        step_time = self.replay_params.step_time_microseconds

        param_section = \
            [[sg.Column([[sg.Text('Trigger events:          ')]]),
              sg.Column(trigger_inputs)],
             [sg.Text('Wait input event:          '),
              sg.Input(default_text=wait_events, key='-WAITINPUTEVENT-')],
             [sg.Text('Wait input topic:          '),
              sg.Input(default_text=wait_topics, key='-WAITINPUTTOPIC-')],
             [sg.Text('Step time (microseconds):  '),
              sg.Input(default_text=step_time, key='-STEPTIMEMICROSECONDS-')],
             [sg.Text('Speed factor [0, 1]:       '),
              sg.Input(default_text=step_time, key='-SPEEDFACTOR-')],
             [sg.Button('Set Params', key='-SETPARAMS-')]]

        return sim_time_section, run_mode_section, event_section, param_section

    def _write_text_box_values(self):
        led_colour = 'red' if self.is_playback_paused else 'green'
        self.run_state_led.set_led(self.window, led_colour)

        if self.replay_params.run_mode == ReplayParams.RunMode.CONTINUOUS:
            self.window.FindElement('-CONTINUOUS-').Update(True)
        elif self.replay_params.run_mode == ReplayParams.RunMode.SINGLESTEP:
            self.window.FindElement('-SINGLESTEP-').Update(True)
        elif self.replay_params.run_mode == ReplayParams.RunMode.STEPTIME:
            self.window.FindElement('-STEPTIME-').Update(True)

        self.window.FindElement('-STEPTIMEMICROSECONDS-').Update(self.replay_params.step_time_microseconds)
        self.window.FindElement('-SPEEDFACTOR-').Update(self.replay_params.speed_factor)
        self.window.FindElement('-WAITINPUTEVENT-').Update(self.replay_params.wait_input_event_name)
        self.window.FindElement('-WAITINPUTTOPIC-').Update(self.replay_params.wait_input_event_topic)
        self.window.FindElement('-RUNWITHOUTDROPS-').Update(self.replay_params.run_without_drops)

        assert(len(self.replay_params.pipeline_end_trigger_names) <= self.max_num_trigger_events)
        for i in range(self.max_num_trigger_events):
            if i < len(self.replay_params.pipeline_end_trigger_names):
                trigger_name = self.replay_params.pipeline_end_trigger_names[i]
                self.window.FindElement('-TRIGGEREVENT{}-'.format(i)).Update(trigger_name)
            else:
                self.window.FindElement('-TRIGGEREVENT{}-'.format(i)).Update('')

    def _update_params(self):
        sim_time = self.remote_control.get_sim_time()

        if sim_time:
            sim_time_seconds = sim_time * 10 ** -6
            self.window.FindElement('-SIMTIMETEXT-').Update("{:.1f}".format(sim_time_seconds))

        new_replay_params = self.remote_control.get_replay_params()

        # Only update window if parameters have changed.
        if new_replay_params != self.replay_params:
            self.replay_params = copy.deepcopy(new_replay_params)
            self._write_text_box_values()

    def run_event_loop(self):
        while True:
            self._update_params()
            event, values = self.window.read(timeout=100)

            if event in (None, 'Exit'):
                plaback_modifier = PlaybackModifier(PlaybackModifier.FINISH)
                self.remote_control.set_playback_modifier(plaback_modifier)
                break

            # ** Events **
            if event == '-PAUSE-':
                plaback_modifier = PlaybackModifier(PlaybackModifier.PAUSE)
                self.remote_control.set_playback_modifier(plaback_modifier)
                self.is_playback_paused = True
                led_colour = 'red' if self.is_playback_paused else 'green'
                self.run_state_led.set_led(self.window, led_colour)

            elif event == '-RESUME-':
                plaback_modifier = PlaybackModifier(PlaybackModifier.RESUME)
                self.remote_control.set_playback_modifier(plaback_modifier)
                self.is_playback_paused = False
                led_colour = 'red' if self.is_playback_paused else 'green'
                self.run_state_led.set_led(self.window, led_colour)

            elif event == '-STEPONCE-':
                plaback_modifier = PlaybackModifier(PlaybackModifier.STEPONCE)
                self.remote_control.set_playback_modifier(plaback_modifier)

            elif event == '-RUNWITHOUTDROPS-':
                self.replay_params.run_without_drops = values['-RUNWITHOUTDROPS-']
                self.remote_control.set_replay_params(self.replay_params)

            # ** Modes **
            elif event == '-CONTINUOUS-':
                self.replay_params.run_mode = ReplayParams.RunMode.CONTINUOUS
                self.remote_control.set_replay_params(self.replay_params)

            elif event == '-SINGLESTEP-':
                self.replay_params.run_mode = ReplayParams.RunMode.SINGLESTEP
                self.remote_control.set_replay_params(self.replay_params)

            elif event == '-STEPTIME-':
                self.replay_params.run_mode = ReplayParams.RunMode.STEPTIME
                self.remote_control.set_replay_params(self.replay_params)

            elif event == '-SETPARAMS-':
                trigger_event_list = [
                    values['-TRIGGEREVENT{}-'.format(i)]
                    for i in range(self.max_num_trigger_events)
                        if values['-TRIGGEREVENT{}-'.format(i)] != ''
                ]
                self.replay_params.pipeline_end_trigger_names = trigger_event_list
                self.replay_params.wait_input_event_name = values[
                    '-WAITINPUTEVENT-']
                self.replay_params.wait_input_event_topic = values[
                    '-WAITINPUTTOPIC-']
                self.replay_params.step_time_microseconds = int(values[
                    '-STEPTIMEMICROSECONDS-'])
                self.replay_params.speed_factor = float(values[
                    '-SPEEDFACTOR-'])
                self.remote_control.set_replay_params(self.replay_params)

        self.window.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('-ip', '--ip', action='store', default=DEFAULT_IP,
                        dest='ip', type=str, help='target IP address')

    parser.add_argument('-port', '--port', action='store', default=DEFAULT_PORT,
                        dest='port', type=int,
                        help='target portd')

    args = parser.parse_args()

    remote_control = RemoteControl()
    remote_control.connect(args.ip, args.port)

    replay_control_gui = ReplayControlGui(remote_control)
    replay_control_gui.run_event_loop()
