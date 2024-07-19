"""
MCF node tree viewing script

Copyright (c) 2024 Accenture
"""


import argparse
import dash
import dash_cytoscape as cyto
import dash_core_components as dcc
import dash_html_components as html
from dash.dependencies import Input, Output, State
import json

import mcf_python_path.mcf_paths
from mcf import Connection
from component_tracing.event_graph import parse_input_events
from component_tracing.event_graph import extract_comps_info
from component_tracing.event_graph import get_values
from component_tracing.event_graph import get_connected_topics
from component_tracing.event_graph import get_two_way_topics

from mcf import RecordReader


def create_comp_nodes(comps_info, filtered_comps):
    """
    Define data elements for components
    """

    nodes = []
    for component in comps_info:
        comp_name = component.get('name')

        # skip disabled components
        if comp_name not in filtered_comps:
            continue

        node_id = 'comp_' + comp_name
        node = {'data': {'id': node_id, 'label': comp_name}, 'classes': 'component'}
        nodes.append(node)

    return nodes


def create_values_nodes(filtered_values):
    """
    Define data elements for values
    """
    nodes = []
    for topic in filtered_values:
        node_id = 'value_' + topic
        topic_broken = '/\n'.join(topic.strip('/').split('/'))
        node = {'data': {'id': node_id, 'label': topic_broken}, 'classes': 'value'}
        nodes.append(node)

    return nodes


def create_edges(comps_info, comps_filtered, values_filtered):
    """
    Define data elements for component to value connections
    """
    edges = []
    for component in comps_info:
        comp_name = component.get('name')

        # skip hidden components
        if comp_name not in comps_filtered:
            continue

        comp_id = 'comp_' + comp_name
        ports = component.get('ports')

        for port in ports:

            # determine port details
            topic = port.get('topic')
            value_id = 'value_' + topic
            connected = port.get('connected')
            direction = port.get('direction')

            # skip hidden or unconnected ports
            if topic not in values_filtered or not connected:
                continue

            if direction == 'sender':
                edge = {'data': {'source': comp_id, 'target': value_id}}
            else:
                edge = {'data': {'source': value_id, 'target': comp_id}}

            edges.append(edge)

    return edges


#
# The component viewer app
#

comp_viewer_stylesheet = [
    # Group selectors
    {'selector': 'node', 'style': {'label': 'data(label)', 'text-wrap': 'wrap'}},
    {'selector': 'edge', 'style': {'curve-style': 'bezier', 'mid-target-arrow-shape': 'triangle', 'arrow-scale': 1.5}},

    # Class selectors
    {'selector': '.component', 'style': {'shape': 'square', 'border-color': 'blue', 'border-width': '2px',
                                         'background-color': 'blue',  'background-blacken': -0.8,
                                         'text-halign': 'center', 'text-valign': 'center',
                                         'height': 'label', 'width': 'label', 'padding': '10px',
                                         'font-size': 14}},
    {'selector': '.value', 'style': {'shape': 'circle', 'border-color': 'green', 'border-width': '2px',
                                     'background-color': 'green',  'background-blacken': -0.8,
                                     'text-halign': 'center', 'text-valign': 'center',
                                     'height': 'label', 'width': 'label', 'padding': '10px',
                                     'font-size': 10}}
]


cyto.load_extra_layouts()
comp_viewer_app = dash.Dash(__name__)

comp_viewer_app.layout = html.Div([
    html.Div([
        html.Div(
            cyto.Cytoscape(
                id='component_tree',
                stylesheet=comp_viewer_stylesheet,
                layout={'name': 'dagre'},
                style={'width': '100%', 'height': '100%', 'nodeDimensionsIncludeLabels': 'true'}),
            style={'float': 'right', 'width': '80%', 'height': '800px'}),
        html.Div([
            html.P("Components:"),
            dcc.Checklist(
                id='component_selector',
                value=[],
                persistence=True,
                labelStyle={'display': 'block'}),
            html.P("Values:"),
            dcc.Checklist(
                id='value_selector',
                options=[
                    {'label': 'Show unconnected values', 'value': 'SHOW_UNCONNECTED'},
                    {'label': 'Show two-ways connected values only', 'value': 'TWO_WAY_ONLY'}
                ],
                value=[],
                persistence=True,
                labelStyle={'display': 'block'}),
            html.P(),
            html.Button('Refresh & Layout', id='refresh')], style={'float': 'left', 'width': '20%'})
    ]),
    # Hidden div to store component info
    html.Div(id='comps_info', style={'display': 'none'}),
])


# refresh component info from target
@comp_viewer_app.callback(
    Output('comps_info', 'children'),
    Input('refresh', 'n_clicks')
)
def refresh(_):

    # if remote connection available, get data from target and save it to the specified output file, if any
    if connection is not None:
        comps_info = connection.rc.get_info()
        if args.save_file is not None:
            with open(args.save_file, 'w') as save_file:
                json.dump({'comps_info': comps_info}, save_file)

    # otherwise use info already loaded from file
    else:
        comps_info = loaded_info.get('comps_info', [])

    return json.dumps(comps_info)


# update available check-boxes for hiding components
@comp_viewer_app.callback(
    Output('component_selector', 'options'),
    Input('comps_info', 'children')
)
def refresh_comps_boxes(comps_info_json):
    comps_info = json.loads(comps_info_json)
    all_comps = [c['name'] for c in comps_info]
    all_comps.sort()
    options = []
    for component in all_comps:
        option = {'label': component, 'value': component}
        options.append(option)

    return options


# update graph based on component info and selected filters
@comp_viewer_app.callback(
    Output(component_id='component_tree', component_property='elements'),
    Output(component_id='component_tree', component_property='autoRefreshLayout'),
    Input(component_id='component_selector', component_property='value'),
    Input(component_id='value_selector', component_property='value'),
    Input(component_id='comps_info', component_property='children'),
)
def update_graph(component_selector, value_selector, comps_info_json):

    comps_info = json.loads(comps_info_json)
    values_info = get_values(comps_info)
    all_comps = [c['name'] for c in comps_info]

    comps_filtered = [c for c in all_comps if c in component_selector]

    # filter values to be displayed
    if 'TWO_WAY_ONLY' in value_selector:
        values_filtered = get_two_way_topics(values_info, comps_filtered)
    elif 'SHOW_UNCONNECTED' in value_selector:
        values_filtered = [v for v in values_info.keys()]
    else:
        values_filtered = get_connected_topics(values_info, comps_filtered)

    comp_nodes = create_comp_nodes(comps_info, comps_filtered)
    value_nodes = create_values_nodes(values_filtered)

    edges = create_edges(comps_info, comps_filtered, values_filtered)

    elements = comp_nodes + value_nodes + edges

    # only create new layout, if component info has been updated (i.e. "Refresh" button clicked)
    do_layout = any([t.get('prop_id', None) == 'comps_info.children' for t in dash.callback_context.triggered])
    return elements, do_layout


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('-ip', '--ip', action='store', required=False, default="10.11.0.60",
                        dest='ip', type=str, help='ip address to connect to for image streams')
    parser.add_argument('-port', '--port', action='store', required=False, default="6666",
                        dest='port', type=str, help='port on which images are streamed')
    parser.add_argument('-save', '--save', action='store', required=False, default=None,
                        dest='save_file', type=str, help='json file to which to save remote target info')
    parser.add_argument('-info', '--info', action='store', required=False, default=None,
                        dest='info', type=str, help='json file from which to load remote target info')
    parser.add_argument('-traces', '--traces', action='append', required=False, default=None,
                        dest='events', type=str,
                        help='component trace recording file from which to derive remote target info')
    args = parser.parse_args()

    # if no file to load given, create connection to remote target
    loaded_info = None
    connection = None
    if args.info is None and args.events is None:
        connection = Connection(ip=args.ip, port=args.port)

    elif args.info is not None and args.events is not None:
        raise ValueError("You may specify either an events recording file or a target info file, but not both")

    # if event recording given, load events from it and store results to info file (if specified)
    elif args.events is not None:

        print("Reading and parsing event trace recording ...")

        parsed_events = parse_input_events(args.events)

        print("... done")

        loaded_info = {'comps_info': extract_comps_info(parsed_events)}

        if args.save_file is not None:
            with open(args.save_file, 'w') as out_file:
                json.dump(loaded_info, out_file)

    # otherwise load data from file
    else:
        with open(args.info) as load_file:
            loaded_info = json.load(load_file)

    comp_viewer_app.run_server(debug=True, port=8050)
