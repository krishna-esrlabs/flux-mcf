"""
Event sequence viewing script

Copyright (c) 2024 Accenture
"""


import argparse
import dash
import dash_core_components as dcc
import dash_html_components as html
from dash.dependencies import Input, Output, State
import numpy as np
import json
import pandas as pd
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots
import pathlib

import mcf_python_path.mcf_paths
from mcf import RecordReader
import component_tracing.event_serialization as event_serialization


KNOWN_EVENTS = [
    {'label': 'Port writes', 'value': 'port_write'},
    {'label': 'Port peeks', 'value': 'port_peek'},
    {'label': 'Port reads', 'value': 'port_read'},
    {'label': 'Trigger activation', 'value': 'trigger_act'},
    {'label': 'Trigger execution', 'value': 'trigger_exec'},
    {'label': 'Execution times', 'value': 'exec_time'},
    {'label': 'Remote transfer times', 'value': 'remote_transfer_time'},
    {'label': 'Program flow event', 'value': 'program_flow'}]

KNOWN_EVENT_VALUES = [e['value'] for e in KNOWN_EVENTS]


# colours to use for known event types
EVENT_COLOURS = px.colors.qualitative.Plotly
KNOWN_EVENT_COLOURS = {e: EVENT_COLOURS[i % len(EVENT_COLOURS)] for i, e in enumerate(KNOWN_EVENT_VALUES)}


def get_values(comps_info):
    """
    Get dictionary of value store topics with associated components
    """

    values = {}

    for component in comps_info:
        comp_name = component.get('name')
        ports = component.get('ports')

        for port in ports:

            # determine port details
            topic_name = port.get('topic')
            connected = port.get('connected')
            direction = port.get('direction')

            # add component info to topic
            topic_comp_info = {'comp_name': comp_name, 'connected': connected, 'direction': direction}
            if topic_name not in values:
                values[topic_name] = [topic_comp_info]
            else:
                values[topic_name].append(topic_comp_info)

    return values


def get_connected_topics(values_info, filtered_components):

    topics = []
    for topic, info in values_info.items():

        for comp in info:

            # skip disabled components
            if comp['comp_name'] in filtered_components:
                topics.append(topic)
                break

    return topics


def get_two_way_topics(values_info, filtered_components):

    topics = []
    for topic, info in values_info.items():

        has_sender = False
        has_receiver = False

        for comp in info:

            # skip disabled components
            if comp['comp_name'] not in filtered_components:
                continue

            if comp['direction'] == 'sender':
                has_sender = True
            else:
                has_receiver = True

            if has_sender and has_receiver:
                topics.append(topic)
                break

    return topics


def filter_event_types(events, selector):

    # show selected events and unknown events
    def is_in_selector(evt_type):
        return evt_type in selector or evt_type not in KNOWN_EVENT_VALUES

    mask_array = []
    for _, desc in events.iterrows():
        mask_array.append(is_in_selector(desc['type']))

    result = events.loc[mask_array]

    return result


def create_nodes(comps_info, filtered_comps, filtered_values):
    """
    Define data elements for components and values
    """

    # collect data of components to show and sort alphabetically
    comps_to_show = []
    for component in comps_info:
        comp_name = component.get('name')

        # skip hidden components
        if comp_name not in filtered_comps:
            continue

        # get ports and sort alphabetically
        ports = component.get('ports', [])
        ports = sorted(ports, key=lambda item: item['topic'])

        # add info to components to show
        node = {'name': comp_name,
                'ports': ports}
        comps_to_show.append(node)

    comps_to_show = sorted(comps_to_show, key=lambda item: item['name'])

    # loop over components and ports to create list of nodes to show
    nodes = []
    for component in comps_to_show:
        comp_name = component.get('name')

        # add component to graph nodes
        node = {'label': comp_name,
                'classes': ['component']}
        nodes.append(node)

        # loop over all related ports
        ports = component['ports']
        for port in ports:

            # determine port details
            value = port.get('topic')
            # direction = port.get('direction')

            # skip hidden values
            if value not in filtered_values:
                continue

            # add value to graph nodes
            node = {'label': value,
                    'classes': ['value']}
            nodes.append(node)

    return nodes


# determine event IDs from selected data
def get_selected_event_ids(selected_data):

    if selected_data is None:
        return []

    selected_customdata = [p.get('customdata', None) for p in selected_data.get('points', [])]
    selected_ids = [d.get('evt_idx', None) for d in selected_customdata]
    selected_ids = [e for e in selected_ids if e is not None]
    return selected_ids


# determine curve IDs in figure from event IDs
def get_selected_curve_ids(figure, event_ids):

    if figure is None or event_ids is None or len(event_ids) == 0:
        return []

    data = figure.data
    selected_ids = [i for i in range(len(data))
                    if data[i].customdata is not None and data[i].customdata[0]['evt_idx'] in event_ids]

    return selected_ids


# determine horizontal plot range from figure re-layout data
def get_fig_xrange(relayout_data, xrange_default):

    xrange = xrange_default
    if relayout_data is not None:
        if 'xaxis.range' in relayout_data:
            xrange = relayout_data['xaxis.range']
        elif 'xaxis.range[0]' in relayout_data:
            xrange = [relayout_data['xaxis.range[0]'], relayout_data['xaxis.range[1]']]

    return xrange


# determine list of selected legend entries from figure re-style_data
def get_selected_legend_entries(restyle_data, visible_entries, all_entries):

    all_entries = list(all_entries)
    visible_entries = set(visible_entries)

    # check if required info is available and consistent
    if restyle_data is not None and len(restyle_data) > 1 and 'visible' in restyle_data[0]:
        visible = restyle_data[0]['visible']
        changed_idx = restyle_data[1]

        # determine set of visible components
        for i in range(len(visible)):
            entry = all_entries[changed_idx[i]]
            if str(visible[i]) == 'True':
                visible_entries.add(entry)
            else:
                visible_entries.discard(entry)

    # bring sequence of visible entries back into original order
    visible_entries = [e for e in all_entries if e in visible_entries]

    return visible_entries


#
# The event sequence viewer app
#


CONNECTOR_STYLES_UNSELECTED = {
    'PORT_READ_CONNECTED': {'line': {'color': 'blue', 'dash': 'solid'},
                            'mode': 'markers',
                            'selected': {'marker': {'opacity': 1}},
                            'unselected': {'marker': {'opacity': 1}},
                            'marker': {'symbol': 'diamond-tall-open',
                                       'size': [6, 6],
                                       'color': ['green', 'green'],
                                       'line': {'color': 'green', 'width': 1}}},
    'PORT_READ_UNCONNECTED': {'line': {'color': 'blue', 'dash': 'dash'},
                              'mode': 'markers',
                              'selected': {'marker': {'opacity': 1}},
                              'unselected': {'marker': {'opacity': 1}},
                              'marker': {'symbol': 'diamond-tall-open',
                                         'size': [6, 6],
                                         'color': ['green', 'green'],
                                         'line': {'color': 'green', 'width': 1}}},
    'PORT_WRITE_CONNECTED': {'line': {'color': 'blue', 'dash': 'solid'},
                             'mode': 'markers',
                             'selected': {'marker': {'opacity': 1}},
                             'unselected': {'marker': {'opacity': 1}},
                             'marker': {'symbol': 'diamond-tall',
                                        'size': [6, 6],
                                        'color': ['green', 'green'],
                                        'line': {'color': 'green', 'width': 1}}},
    'PORT_WRITE_UNCONNECTED': {'line': {'color': 'blue', 'dash': 'dash'},
                               'mode': 'markers',
                               'selected': {'marker': {'opacity': 1}},
                               'unselected': {'marker': {'opacity': 1}},
                               'marker': {'symbol': 'diamond-tall',
                                          'size': [6, 6],
                                          'color': ['green', 'green'],
                                          'line': {'color': 'green', 'width': 1}}},
    'TRIGGER_ACT': {'line': {'color': 'blue', 'dash': 'dot'},
                    'mode': 'markers',
                    'selected': {'marker': {'opacity': 1}},
                    'unselected': {'marker': {'opacity': 1}},
                    'marker': {'symbol': 'arrow-down',
                               'color': 'blue',
                               'size': [0, 6],
                               'line': {'color': 'blue', 'width': 1}}},
    'TRIGGER_EXEC_1': {'line': {'color': 'blue', 'dash': 'dot'},
                       'mode': 'markers',
                       'selected': {'marker': {'opacity': 1}},
                       'unselected': {'marker': {'opacity': 1}},
                       'marker': {'symbol': 'line-ns',
                                  'color': 'blue',
                                  'size': [0, 10],
                                  'line': {'color': 'blue', 'width': 1}}},
    'TRIGGER_EXEC_2': {'line': {'color': 'blue', 'dash': 'solid', 'width': 2},
                       'mode': 'lines+markers',
                       'selected': {'marker': {'opacity': 1}},
                       'unselected': {'marker': {'opacity': 1}},
                       'marker': {'symbol': 'line-ns',
                                  'color': 'blue',
                                  'size': [10, 10],
                                  'line': {'color': 'blue', 'width': 1}}},
    'EXEC_TIME': {'line': {'color': 'blue', 'dash': 'dot', 'width': 2},
                  'mode': 'lines+markers',
                  'selected': {'marker': {'opacity': 1}},
                  'unselected': {'marker': {'opacity': 1}},
                  'marker': {'symbol': 'line-ns',
                             'color': 'blue',
                             'size': [10, 10],
                             'line': {'color': 'blue', 'width': 1}}},
    'REMOTE_TRANSFER_TIME': {'line': {'color': 'blue', 'dash': 'dash', 'width': 2},
                  'mode': 'lines+markers',
                  'selected': {'marker': {'opacity': 1}},
                  'unselected': {'marker': {'opacity': 1}},
                  'marker': {'symbol': 'line-ns',
                             'color': 'blue',
                             'size': [10, 10],
                             'line': {'color': 'blue', 'width': 1}}},
    'PROGRAM_FLOW': {'line': {'color': 'blue', 'dash': 'solid'},
                     'mode': 'markers',
                     'selected': {'marker': {'opacity': 1}},
                     'unselected': {'marker': {'opacity': 1}},
                     'marker': {'symbol': 'diamond-tall',
                                'size': [6, 6],
                                'color': ['orange', 'orange'],
                                'line': {'color': 'orange', 'width': 1}}},
    'UNKNOWN': {'line': {'color': 'blue', 'dash': 'dot', 'width': 1},
                'mode': 'markers',
                'selected': {'marker': {'opacity': 1}},
                'unselected': {'marker': {'opacity': 1}},
                'marker': {'symbol': 'line-ns',
                           'color': 'blue',
                           'size': [10, 0],
                           'line': {'color': 'blue', 'width': 1}}}
}

CONNECTOR_STYLES_SELECTED = {
    'PORT_READ_CONNECTED': {'line': {'color': 'red', 'dash': 'solid'},
                            'mode': 'lines+markers',
                            'selected': {'marker': {'opacity': 1}},
                            'unselected': {'marker': {'opacity': 1}},
                            'marker': {'symbol': 'diamond-tall-open',
                                       'size': [6, 6],
                                       'color': 'red',
                                       'line': {'color': 'red', 'width': 1}}},
    'PORT_READ_UNCONNECTED': {'line': {'color': 'red', 'dash': 'dash'},
                              'mode': 'lines+markers',
                              'selected': {'marker': {'opacity': 1}},
                              'unselected': {'marker': {'opacity': 1}},
                              'marker': {'symbol': 'diamond-tall-open',
                                         'size': [6, 6],
                                         'color': 'red',
                                         'line': {'color': 'red', 'width': 1}}},
    'PORT_WRITE_CONNECTED': {'line': {'color': 'red', 'dash': 'solid'},
                             'mode': 'lines+markers',
                             'selected': {'marker': {'opacity': 1}},
                             'unselected': {'marker': {'opacity': 1}},
                             'marker': {'symbol': 'diamond-tall',
                                        'size': [6, 6],
                                        'color': 'red',
                                        'line': {'color': 'red', 'width': 1}}},
    'PORT_WRITE_UNCONNECTED': {'line': {'color': 'red', 'dash': 'dash'},
                               'mode': 'lines+markers',
                               'selected': {'marker': {'opacity': 1}},
                               'unselected': {'marker': {'opacity': 1}},
                               'marker': {'symbol': 'diamond-tall',
                                          'size': [6, 6],
                                          'color': 'red',
                                          'line': {'color': 'red', 'width': 1}}},
    'TRIGGER_ACT': {'line': {'color': 'red', 'dash': 'dot'},
                    'mode': 'lines+markers',
                    'selected': {'marker': {'opacity': 1}},
                    'unselected': {'marker': {'opacity': 1}},
                    'marker': {'symbol': 'arrow-down',
                               'color': 'red',
                               'size': [0, 6],
                               'line': {'color': 'red', 'width': 1}}},
    'TRIGGER_EXEC_1': {'line': {'color': 'red', 'dash': 'dot'},
                       'mode': 'lines+markers',
                       'selected': {'marker': {'opacity': 1}},
                       'unselected': {'marker': {'opacity': 1}},
                       'marker': {'symbol': 'line-ns',
                                  'color': 'red',
                                  'size': [0, 10],
                                  'line': {'color': 'red', 'width': 1}}},
    'TRIGGER_EXEC_2': {'line': {'color': 'red', 'dash': 'solid', 'width': 2},
                       'mode': 'lines+markers',
                       'selected': {'marker': {'opacity': 1}},
                       'unselected': {'marker': {'opacity': 1}},
                       'marker': {'symbol': 'line-ns',
                                  'color': 'red',
                                  'size': [10, 10],
                                  'line': {'color': 'red', 'width': 1}}},
    'EXEC_TIME': {'line': {'color': 'red', 'dash': 'dot', 'width': 2},
                  'mode': 'lines+markers',
                  'selected': {'marker': {'opacity': 1}},
                  'unselected': {'marker': {'opacity': 1}},
                  'marker': {'symbol': 'line-ns',
                             'color': 'red',
                             'size': [10, 10],
                             'line': {'color': 'red', 'width': 1}}},
    'REMOTE_TRANSFER_TIME': {'line': {'color': 'red', 'dash': 'dash', 'width': 2},
                  'mode': 'lines+markers',
                  'selected': {'marker': {'opacity': 1}},
                  'unselected': {'marker': {'opacity': 1}},
                  'marker': {'symbol': 'line-ns',
                             'color': 'red',
                             'size': [10, 10],
                             'line': {'color': 'red', 'width': 1}}},
    'PROGRAM_FLOW': {'line': {'color': 'red', 'dash': 'solid'},
                     'mode': 'lines+markers',
                     'selected': {'marker': {'opacity': 1}},
                     'unselected': {'marker': {'opacity': 1}},
                     'marker': {'symbol': 'diamond-tall',
                                'size': [6, 6],
                                'color': 'red',
                                'line': {'color': 'red', 'width': 1}}},
    'UNKNOWN': {'line': {'color': 'red', 'dash': 'dot', 'width': 1},
                'mode': 'markers',
                'selected': {'marker': {'opacity': 1}},
                'unselected': {'marker': {'opacity': 1}},
                'marker': {'symbol': 'line-ns',
                           'color': 'red',
                           'size': [10, 0],
                           'line': {'color': 'red', 'width': 1}}}
}


sequence_viewer_app = dash.Dash(__name__)

sequence_viewer_app.layout = html.Div([
    html.Div(
        [
            html.Div([

                html.P(html.B("Component and time range pre-selection:")),
                html.Div(
                    dcc.Checklist(
                        id='component_selector', options=[], persistence='session', value=[],
                        labelStyle={'display': 'block'}, style={'fontSize': 14}),
                    style={'width': '20%', 'float': 'left'}),

                html.Div([
                    dcc.Loading(dcc.Graph(id='full_preview'))
                ], style={'width': '80%', 'float': 'right'}),
            ], style={'width': '100%', 'overflow': 'hidden'}),

            html.Div([

                html.P(html.B("Event overview:")),
                dcc.Loading(dcc.Graph(id='detailed_preview',
                                      config={'modeBarButtonsToRemove': ['select2d', 'lasso2d', 'autoScale2d',
                                                                         'resetScale2d', 'toggleSpikelines',
                                                                         'hoverCompareCartesian',
                                                                         'hoverClosestCartesian']})),
                html.P(html.B("Detailed view:")),
                dcc.Checklist(
                    id='event_selector',
                    options=KNOWN_EVENTS,
                    persistence='session',
                    value=[]),
                html.P(id='evt_count_out'),
                html.P(["Max number of events to plot (<=0 for all): ",
                        dcc.Input(id="max_evt_count", type="number", placeholder="number", debounce=True, value=500)]),
                html.Button('Create plot', id='plot_details'),
                html.P(dcc.Checklist(
                    id='value_selector',
                    options=[
                        {'label': 'Two-ways connected values only     ', 'value': 'TWO_WAY_ONLY'},
                        {'label': 'No values', 'value': 'NONE'}
                    ],
                    persistence=True,
                    value=[])),
                dcc.Loading(dcc.Graph(id='graph')),
                html.P(html.B("Clicked event:")),
                html.Pre(id='clicked_categ'),
                html.Pre(id='clicked_event')
            ])
        ]),
    # Hidden divs to store status info
    html.Div(id='db_meta_info', style={'display': 'none'}, children="{}"),
    html.Div(id='prev_selected_info', style={'display': 'none'}, children="[]"),
    html.Div(id='selected_info', style={'display': 'none'}, children="[]"),
    html.Div(id='presel_xrange', style={'display': 'none'}, children="{}"),
    html.Div(id='visible_xrange', style={'display': 'none'}, children="{}"),
    html.Div(id='visible_xrange_feedback', style={'display': 'none'}, children="{}"),
    html.Div(id='full_preview_style', style={'display': 'none'}, children="{}"),
    html.Div(id='hires_xrange', style={'display': 'none'}, children="{}"),
    html.Div(id='dummy', style={'display': 'none'})
])


# refresh component info from target
@sequence_viewer_app.callback(
    Output('db_meta_info', 'children'),
    Input('dummy', 'id')
)
def set_component_info(_):
    # comps_info = connection.rc.get_info()
    db_meta_info = db_meta_data
    return json.dumps(db_meta_info)


# create custom data for connector of given type and event index
def connector_data(evt_type, evt_idx):
    return {'type': evt_type, 'evt_idx': evt_idx}


# update component selector
@sequence_viewer_app.callback(
    Output('component_selector', 'options'),
    Input(component_id='db_meta_info', component_property='children'),
)
def update_comp_selector(db_meta_info_json):

    db_meta_info = json.loads(db_meta_info_json)
    comps_info = db_meta_info.get('comps_info', {})
    all_comps = [c['name'] for c in comps_info]
    all_comps.sort()

    options = []
    for component in all_comps:
        option = {'label': component, 'value': component}
        options.append(option)

    return options


# update full preview based on component info and selected filters
@sequence_viewer_app.callback(
    Output(component_id='full_preview', component_property='figure'),
    Output(component_id='full_preview_style', component_property='children'),
    Input(component_id='component_selector', component_property='value'),
    Input(component_id='visible_xrange_feedback', component_property='children')
)
def update_full_preview(selected_comps, visible_xrange_json):

    selected_comps.sort()
    if len(selected_comps) == 0:
        return go.Figure(), json.dumps({'trace_colours': {}})

    totals = {}
    for cat in selected_comps:
        counts_per_type = event_densities_by_categ.get(cat, None)
        if counts_per_type is None:
            continue
        tot = counts_per_type.sum(axis=1)
        tot.index = pd.to_numeric(tot.index) / 1.e6
        totals[cat] = tot

    fig = px.area(totals)
    fig.update_xaxes(showgrid=True, title='time [ms]')
    fig.update_yaxes(showgrid=True, title='events / s')
    fig.update_layout(xaxis=dict(rangeslider=dict(visible=True)))

    # adapt zoom, if visible xrange has changed
    visible_xrange = json.loads(visible_xrange_json)
    xrange = visible_xrange.get('xrange', None)

    if xrange is None:
        fig.update_xaxes(autorange=True)
    else:
        fig.update_xaxes(range=xrange)

    # create dictionary of component traces' colours (from list of default colours)
    colours = px.colors.qualitative.Plotly
    trace_colours = {comp: colours[i % len(colours)] for i, comp in enumerate(selected_comps)}

    return fig, json.dumps({'trace_colours': trace_colours})


# determine pre-selected xrange from full preview
@sequence_viewer_app.callback(
    Output(component_id='presel_xrange', component_property='children'),
    Input(component_id='db_meta_info', component_property='children'),
    Input(component_id='full_preview', component_property='relayoutData'),
    State(component_id='presel_xrange', component_property='children'),
)
def update_presel_xrange(db_meta_info_json, layout_data, presel_xrange_json):

    db_meta_info = json.loads(db_meta_info_json)
    full_xrange = db_meta_info.get('time_range', [0, 0])

    # determine current x-range from full preview
    presel_xrange = json.loads(presel_xrange_json)
    xrange = presel_xrange.get('xrange', full_xrange)

    # determine range from full preview
    xrange = get_fig_xrange(layout_data, xrange)

    return json.dumps({'xrange': xrange})

# update detailed preview
@sequence_viewer_app.callback(
    Output(component_id='detailed_preview', component_property='figure'),
    Input(component_id='component_selector', component_property='value'),
    Input(component_id='full_preview_style', component_property='children'),
    Input(component_id='visible_xrange', component_property='children'),
)
def update_detailed_preview(vis_comps, full_preview_style_json,
                            visible_xrange_json):

    # get dictionary of trace colours used in full preview
    full_preview_style = json.loads(full_preview_style_json)
    trace_colours = full_preview_style.get('trace_colours', {})

    # determine visible components and events as well as current x-range from full preview
    visible_xrange = json.loads(visible_xrange_json)
    xrange = visible_xrange.get('xrange', None)

    if len(vis_comps) == 0:
        return go.Figure()

    fig_height = 150 + 70 * len(vis_comps)
    fig = make_subplots(rows=len(vis_comps),
                        cols=1,
                        shared_xaxes=True,
                        vertical_spacing=28./(fig_height-170),
                        specs=[[{"type": "scatter"}] for _ in range(len(vis_comps))],
                        subplot_titles=vis_comps)

    fig.update_layout(height=fig_height, uirevision=0)

    legend_entries = []
    for i in range(len(vis_comps)):
        cat = vis_comps[i]
        yaxis = 'y' if i == 1 else f'y{i+1}'
        colour = trace_colours.get(cat, 'rgb(0,0,0)')
        traces, legend_entries = make_detailed_subfig(cat, yaxis, xrange, colour, legend_entries=legend_entries)

        for trace in traces:
            fig.add_trace(trace, row=i+1, col=1)

    fig.update_xaxes(showgrid=True)
    fig.update_yaxes(ticks='', showticklabels=False, showgrid=False, zeroline=False)
    # fig.update_layout(height=fig_height, legend_orientation='h', legend_yanchor="bottom", legend_y=1.1)

    for annotation in fig['layout']['annotations']:
        annotation['font']['size'] = 10
        annotation['x'] = 0.
        annotation['xanchor'] = 'left'
        annotation['xref'] = 'paper'

    if xrange is None:
        fig.update_xaxes(autorange=True)
    else:
        fig.update_xaxes(range=xrange, autorange=False)

    return fig


def make_detailed_subfig(category, yaxis, xrange, colour, legend_entries):

    counts_per_type = event_counts_hires.get(category, None)
    if counts_per_type is None:
        return [], legend_entries

    # select requested range, if if specified; otherwise use full range
    if xrange is not None:
        xrange = [xrange[0] * 1e6, xrange[1] * 1e6]  # convert to ns
        counts_per_type = counts_per_type.loc[pd.Timestamp(xrange[0]): pd.Timestamp(xrange[-1])]
    else:
        xrange = [0, max_time * 1e6]

    # resample to required resolution based on range
    time_res = (xrange[1] - xrange[0]) // 200
    counts_per_type = counts_per_type.resample(f'{time_res}ns').sum().fillna(0)

    # loop over all event types
    traces = []

    for evt_type in counts_per_type.columns:

        # determine index of event type, use max, if event type is not in list of known events
        index = [i for i, name in enumerate(KNOWN_EVENT_VALUES) if name == evt_type]
        index = index[0] if len(index) > 0 else len(KNOWN_EVENT_VALUES)

        # determine colour of event type
        evt_colour = KNOWN_EVENT_COLOURS.get(evt_type, 'rgba(0, 0, 0, 1)')

        # add event type to legend, if it is not yet there
        show_legend = False
        if evt_type not in legend_entries:
            show_legend = True

        # keeping all legend entries (even duplicates) is required for proper handling of mouse clicks on legend
        legend_entries.append(evt_type)

        # plot events
        x = pd.to_numeric(counts_per_type.index).to_numpy()
        counts = counts_per_type[evt_type].to_numpy()
        x = x[counts > 0]
        y = np.full(len(x), index)

        trace = go.Scatter(x=x / 1.e6,  # convert back to ms
                           y=y,
                           mode='markers',
                           name=evt_type,
                           legendgroup=evt_type,
                           yaxis=yaxis,
                           marker_color=evt_colour,
                           showlegend=show_legend)
        traces.append(trace)

    return traces, legend_entries


# determine time range (x-axis) from detailed preview
def _update_visible_xrange_helper(layout_data, presel_xrange_json):

    # if preselection from full preview has changed, use it and ignore layout data from detailed preview
    triggers = set([t['prop_id'] for t in dash.callback_context.triggered])
    if triggers & {'presel_xrange.children'}:
        presel_xrange = json.loads(presel_xrange_json)
        xrange = presel_xrange.get('xrange', None)
        return json.dumps({'xrange': xrange})

    # determine range from full preview
    xrange = get_fig_xrange(layout_data, None)

    # do not update visible xrange if no xrange in input (i.e. prevent auto-rescaling)
    if xrange is None:
        raise dash.exceptions.PreventUpdate

    return json.dumps({'xrange': xrange})


# determine time range (x-axis) from detailed preview
@sequence_viewer_app.callback(
    Output(component_id='visible_xrange', component_property='children'),
    Input(component_id='detailed_preview', component_property='relayoutData'),
    Input(component_id='presel_xrange', component_property='children'),
)
def update_visible_xrange(layout_data, presel_xrange_json):
    return _update_visible_xrange_helper(layout_data, presel_xrange_json)


# determine time range (x-axis) from detailed preview for feedback to full preview
@sequence_viewer_app.callback(
    Output(component_id='visible_xrange_feedback', component_property='children'),
    Input(component_id='detailed_preview', component_property='relayoutData'),
    State(component_id='presel_xrange', component_property='children'),
)
def update_visible_xrange_feedback(layout_data, presel_xrange_json):
    return _update_visible_xrange_helper(layout_data, presel_xrange_json)


# determine total number of events within the visible range
@sequence_viewer_app.callback(
    Output(component_id='evt_count_out', component_property='children'),
    Input(component_id='visible_xrange', component_property='children'),
    Input(component_id='component_selector', component_property='value'),
    Input(component_id='event_selector', component_property='value'),
)
def update_eventcount(visible_xrange_json, vis_comps, vis_events):

    # get visible xrange
    visible_xrange = json.loads(visible_xrange_json)
    xrange = visible_xrange.get('xrange')

    if xrange is None:
        xrange = [0, 0]

    start_time = pd.Timestamp(xrange[0], unit='ms')
    end_time = pd.Timestamp(xrange[1], unit='ms')

    event_counts = sum_events(event_counts_hires, vis_comps, vis_events)
    count = event_counts[start_time:end_time].sum()

    return f'Number of events in selected time range: {int(count)}'


# update time range to plot in high-res event graph
@sequence_viewer_app.callback(
    Output(component_id='hires_xrange', component_property='children'),
    Input(component_id='visible_xrange', component_property='children'),
    Input(component_id='component_selector', component_property='value'),
    Input(component_id='event_selector', component_property='value'),
    Input(component_id='max_evt_count', component_property='value'),
)
def update_xrange_hires_graph(visible_xrange_json, vis_comps, vis_events, max_evt_count):

    # get visible xrange
    visible_xrange = json.loads(visible_xrange_json)
    xrange = visible_xrange.get('xrange')

    if xrange is None:
        xrange = [0, 0]

    start_time = pd.Timestamp(xrange[0], unit='ms')
    end_time = pd.Timestamp(xrange[1], unit='ms')

    event_counts = sum_events(event_counts_hires, vis_comps, vis_events)
    cumul_counts = event_counts[start_time:end_time].cumsum(axis=0)

    if len(cumul_counts) == 0:
        return json.dumps({'xrange': [xrange[0], xrange[1]]})

    if max_evt_count is None:
        max_evt_count = 1

    end_index = -1
    full_count = cumul_counts.iloc[end_index]
    if 0 < max_evt_count < full_count:
        end_index = np.argmax(cumul_counts.to_numpy() > max_evt_count)

    end_time = cumul_counts.index[end_index]

    # Set the end time to be the time end time of the final bin
    time_increment = event_counts.index[1] - event_counts.index[0]
    end_time_ms = ((end_time + time_increment).to_numpy().astype(int)) / 1.e6

    return json.dumps({'xrange': [xrange[0], end_time_ms]})


# update graph based on previews
@sequence_viewer_app.callback(
    Output(component_id='graph', component_property='figure'),
    Output(component_id='selected_info', component_property='children'),
    Input('plot_details', 'n_clicks'),
    Input(component_id='value_selector', component_property='value'),
    Input(component_id='graph', component_property='selectedData'),
    State(component_id='db_meta_info', component_property='children'),
    State(component_id='graph', component_property='figure'),
    State(component_id='selected_info', component_property='children'),
    State(component_id='component_selector', component_property='value'),
    State(component_id='event_selector', component_property='value'),
    State(component_id='visible_xrange', component_property='children'),
    State(component_id='hires_xrange', component_property='children')
)
def update_graph(_, value_selector, selected_data, db_meta_info_json, figure,
                 selected_info_json, vis_comps, vis_events,
                 visible_xrange_json, hires_xrange_json):

    print("updating graph")

    triggers = [t['prop_id'] for t in dash.callback_context.triggered]

    # determine subset of triggers that require to re-create the figure
    def recreate_triggers():
        return set(triggers) & {'plot_details.n_clicks'}

    # determine subset of triggers that require all events to be redrawn
    def redraw_all_triggers():
        return set(triggers) & {'component_selector.value',
                                'value_selector.value',
                                'event_selector.value',
                                'db_meta_info.children',
                                'time_presel.value'}

    # determine subset of triggers that require selected events to be redrawn
    def redraw_selected_triggers():
        return set(triggers) & {'graph.selectedData'}

    # get info about components and values
    db_meta_info = json.loads(db_meta_info_json)
    comps_info = db_meta_info.get('comps_info', {})
    all_comps = [c['name'] for c in comps_info]
    all_comps.sort()
    values_info = get_values(comps_info)

    # determine visible xrange
    visible_xrange = json.loads(visible_xrange_json)
    xrange = visible_xrange.get('xrange', [0, 0])

    # determine xrange of events to plot
    hires_xrange = json.loads(hires_xrange_json)
    xrange_to_plot = hires_xrange.get('xrange', [0, 0])

    if len(all_comps) == 0:
        return go.Figure(), json.dumps([])

    if len(vis_comps) == 0:
        return go.Figure(), json.dumps([])

    if xrange is None:
        xrange = [0, 0]

    # get previously selected data
    prev_selected_curve_ids = json.loads(selected_info_json)

    # filter values to be displayed
    if 'NONE' in value_selector:
        values_filtered = []
    else:
        if 'TWO_WAY_ONLY' in value_selector:
            values_filtered = get_two_way_topics(values_info, vis_comps)
        else:
            values_filtered = get_connected_topics(values_info, vis_comps)

    # determine nodes (entries on y-axis) to show
    nodes = create_nodes(comps_info, vis_comps, values_filtered)

    # create list of strings to display on the y-axis
    categories = [n['label'] for n in nodes[::-1]]
    colors = ['blue' if 'component' in n['classes'] else 'green' for n in nodes[::-1]]

    # determine selected event ids and curve ids
    selected_event_ids = get_selected_event_ids(selected_data)

    print("pre-generating figure")
    fig = go.Figure(figure)
    print("figure pre-generated")

    # if no figure or re-creation required, create it
    need_complete_redraw = False
    need_selected_redraw = False
    if fig is None or recreate_triggers():
        fig = go.Figure([], layout={'title': 'Event timings', 'height': 800, 'uirevision': 0})
        need_complete_redraw = True

    # otherwise redraw all events, if at least one of the corresponding triggers occurred
    elif redraw_all_triggers():
        fig.data = []
        need_complete_redraw = True

    # otherwise check , if selected events need to be redrawn
    elif redraw_selected_triggers():
        need_selected_redraw = True

    selected_curve_ids = get_selected_curve_ids(fig, selected_event_ids)

    # if figure needs to be redrawn completely
    if need_complete_redraw:

        # Set axes ranges
        time_period = xrange[1] - xrange[0]
        axes_time_range = [xrange[0] - 0.05 * time_period, xrange[1] + 0.05 * time_period]
        fig.update_yaxes(type='category', categoryorder='array', categoryarray=categories,
                         showgrid=False, tickfont={'size': 10})  # , tickangle=-45)
        fig.update_xaxes(tickfont={'size': 10}, title='time [ms]', range=axes_time_range)  # , tickangle=-45)

        # define selection interactions
        fig.update_layout(clickmode='event+select')

        # draw horizontal lines, colored according to type of node
        for cat, color in zip(categories, colors):
            fig.add_trace(go.Scatter(x=xrange, y=[cat, cat], line={'color': color},
                                     opacity=0.3, showlegend=False, hoverinfo='none', mode='lines'))

        # delete info about previously selected events
        prev_selected_curve_ids = []

        print("adding scatter traces")

        # add all events of selected categories to plot
        start_time = pd.Timestamp(xrange_to_plot[0], unit='ms')
        end_time = pd.Timestamp(xrange_to_plot[1], unit='ms')

        for categ in categories:
            events = events_by_categ.get(categ, pd.DataFrame())
            events = events.loc[start_time:end_time]

            # filter events based on event selector
            filtered_events = filter_event_types(events, vis_events)

            collected_traces = []
            for time, event in filtered_events.iterrows():

                traces = get_event_traces(time, event, categories, selected_event_ids)
                if traces is not None:
                    collected_traces += traces

            if len(collected_traces) > 0:
                fig.add_traces(collected_traces)

        # indicate that selected events do not need to be redrawn
        need_selected_redraw = False

    # if selected events need to be redrawn
    if need_selected_redraw:

        # update previously and currently selected curve IDs
        data = fig.data

        for curve_id in (prev_selected_curve_ids + selected_curve_ids):
            trace = data[curve_id]

            is_selected = curve_id in selected_curve_ids
            if is_selected:
                conn_styles = CONNECTOR_STYLES_SELECTED
            else:
                conn_styles = CONNECTOR_STYLES_UNSELECTED

            type = trace.customdata[0]['type']
            style = conn_styles[type]

            # update trace style
            trace.update(overwrite=True, **style)

    # shade area of skipped events
    if xrange_to_plot[1] < xrange[1]:
        fig.add_vrect(
            x0=xrange_to_plot[1], x1=xrange[1],
            fillcolor="darkgray", opacity=0.5,
            layer="below", line_width=0,
        )

    # remember curve IDs corresponding to selected events
    selected_info_json = json.dumps(selected_curve_ids)

    print("figure ready")

    return fig, selected_info_json


def get_event_traces(time, event, categories, selected_event_ids):

    def is_event_selected(event_desc):

        if selected_event_ids is None:
            return False

        evt_idx = event_desc['evt_idx']
        return evt_idx in selected_event_ids

    evt_type = event.get('type', None)
    time = (time - pd.Timestamp("1970-01-01")) / pd.Timedelta('1ms')
    categ = event.get('cat', None)
    categ2 = event.get('cat2', categ)  # categ: component, categ2: value (use component as fallback)
    if categ2 not in categories:
        categ2 = categ

    event_index = event.get('evt_idx', None)
    selected = is_event_selected(event)

    traces = None

    # port read/write
    if evt_type in {'port_peek', 'port_read', 'port_write'}:
        connected = event.get('connect', False)

        if evt_type == 'port_read' or evt_type == 'port_peek':
            if connected:
                trace_type = 'PORT_READ_CONNECTED'
            else:
                trace_type = 'PORT_READ_UNCONNECTED'
        else:
            if connected:
                trace_type = 'PORT_WRITE_CONNECTED'
            else:
                trace_type = 'PORT_WRITE_UNCONNECTED'

        trace = get_trace([time, time], [categ, categ2], trace_type, event_index, selected)
        traces = [trace]

    # trigger activation
    elif evt_type == 'trigger_act':
        trace = get_trace([time, time], [categ2, categ], 'TRIGGER_ACT', event_index, selected)
        traces = [trace]

    # trigger execution
    elif evt_type == 'trigger_exec':
        exec_time = event.get('exec_time', 0.) * 1000.  # get execution time in ms
        trigger_time = (event.get('t2') - pd.Timestamp("1970-01-01")) / pd.Timedelta('1ms')
        time_beg = time - exec_time

        trace1 = get_trace([trigger_time, time_beg], [categ2, categ], 'TRIGGER_EXEC_1', event_index, selected)
        trace2 = get_trace([time_beg, time], [categ, categ], 'TRIGGER_EXEC_2', event_index, selected)
        traces = [trace1, trace2]

    # execution time
    elif evt_type == 'exec_time':
        exec_time = event.get('exec_time', 0.) * 1000.  # get execution time in ms
        time_beg = time - exec_time
        trace = get_trace([time_beg, time], [categ, categ], 'EXEC_TIME', event_index, selected)
        traces = [trace]

    # remote transfer time
    elif evt_type == 'remote_transfer_time':
        transfer_time = event.get('remote_transfer_time', 0.) * 1000.  # get execution time in ms
        time_beg = time - transfer_time
        trace = get_trace([time_beg, time], [categ, categ], 'REMOTE_TRANSFER_TIME', event_index, selected)
        traces = [trace]

    # program flow event
    elif evt_type == 'program_flow':
        event_time = event.get('program_flow', 0.) * 1000.  # get execution time in ms
        time_beg = time - event_time
        trace = get_trace([time_beg, time], [categ, categ], 'PROGRAM_FLOW',
                          event_index, selected)
        traces = [trace]

    # unknown event type: plot marker, if time and category are known
    elif categ is not None and time is not None:
        trace = get_trace([time, time], [categ, categ], 'UNKNOWN', event_index, selected)
        traces = [trace]

    return traces


def get_trace(x, y, trace_type, event_index, is_selected):
    """
    Create scatter plot trace of given type with given x, y, and event index data
    """

    if is_selected:
        conn_styles = CONNECTOR_STYLES_SELECTED
    else:
        conn_styles = CONNECTOR_STYLES_UNSELECTED

    style = conn_styles[trace_type]
    customdata = connector_data(trace_type, event_index)

    trace = go.Scatter(x=x, y=y, showlegend=False,
                       customdata=[customdata, customdata], **style)

    return trace


# handle clicks on graph data points
@sequence_viewer_app.callback(
    Output('clicked_categ', 'children'),
    Output('clicked_event', 'children'),
    Input('graph', 'clickData'))
def display_click_data(click_data):

    if click_data is None or len(click_data['points']) == 0:
        return None, None

    point = click_data['points'][0]
    categ = point['y']

    # search entry in data base
    event_index = point['customdata']['evt_idx']

    # if entry exists, output its value
    event = None
    if event_index is not None and 0 <= event_index < len(parsed_events):
        event = parsed_events[event_index]

    categ = str(categ)
    event = json.dumps(event, indent=2)

    return categ, event


def convert_input_event(in_event, event_index, time_correct):
    """
    Convert the given input event into a list of event descriptors for display
    """

    out_tstamp = pd.Timestamp(in_event['timestamp'] - time_correct, unit='us')
    out_type = in_event['type']

    # port i/o event: create corresponding events
    out_list = []
    if out_type == 'port_peek' or out_type == 'port_read' or out_type == 'port_write':

        # create event
        out_event = {
            'type': out_type,
            'cat': in_event['component']['name'],
            'cat2': in_event['port']['topic'],
            'connect': in_event['port']['connected'],
            't': out_tstamp
        }
        out_list.append(out_event)

    elif out_type == 'exec_time':

        # create event
        out_event = {
            'type': out_type,
            'cat': in_event['component']['name'],
            't': out_tstamp,
            'exec_time': in_event['exec_time']
        }
        out_list.append(out_event)

    elif out_type == 'remote_transfer_time':

        # create event
        out_event = {
            'type': out_type,
            'cat': in_event['component']['name'],
            't': out_tstamp,
            'remote_transfer_time': in_event['remote_transfer_time']
        }
        out_list.append(out_event)

    elif out_type == 'trigger_exec':

        # create event
        out_event = {
            'type': out_type,
            'cat': in_event['component']['name'],
            'cat2': in_event['trigger']['topic'],
            't': out_tstamp,
            't2': pd.Timestamp(in_event['trigger']['tstamp'] - time_correct, unit='us'),
            'exec_time': in_event['exec_time']
        }
        out_list.append(out_event)

    elif out_type == 'trigger_act':

        # create event
        out_event = {
            'type': out_type,
            'cat': in_event['component']['name'],
            'cat2': in_event['trigger']['topic'],
            't': pd.Timestamp(in_event['trigger']['tstamp'] - time_correct, unit='us'),
        }
        out_list.append(out_event)

    elif out_type == 'program_flow':

        # create event
        out_event = {
            'type': out_type,
            'cat': in_event['component']['name'],
            't': out_tstamp
        }
        out_list.append(out_event)

    # unknown event type
    else:
        # create event
        out_event = {
            'type': out_type,
            'cat': in_event['component']['name'],
            't': out_tstamp,
        }
        out_list.append(out_event)

    # add index to all output events
    for event in out_list:
        event['evt_idx'] = event_index

    return out_list


def parse_input_events(traces):

    in_events = []
    for trace in traces:
        evt_reader = RecordReader()
        evt_reader.open(trace)

        # open value store recording and filter all entries
        records_generator = evt_reader.records_generator()

        # read all input events, skipping records that are not component trace events
        for record in records_generator:
            event = event_serialization.parse_mcf_event(record)
            if event is not None:
                in_events.append(event)

    sorted_in_events = sorted(in_events, key=lambda event: event['timestamp'])

    return sorted_in_events


def extract_comps_info(events):

    comps_ports = {}
    for event in events:

        # skip events that are not port reads or port writes
        event_type = event['type']
        if event_type != 'port_peek' and event_type != 'port_read' and event_type != 'port_write':
            continue

        # determine component name and port direction
        comp_name = event['component']['name']
        direction = 'sender' if event_type == 'port_write' else 'receiver'

        # get ports info already collected for this component and port direction
        # Note: for each component, we keep at most one port per topic and direction
        comp_ports = comps_ports.get(comp_name, {})
        ports = comp_ports.get(direction, {})

        # add port, if not yet in dictionary of topics for this direction
        topic = event['port']['topic']
        if topic is not None and topic not in ports:

            # get port descriptor from event and add in the port direction
            port_desc = event['port']
            port_desc['direction'] = direction
            ports[topic] = event['port']

        comp_ports[direction] = ports
        comps_ports[comp_name] = comp_ports

    # convert to lists
    result = []
    for comp_name, ports_per_direction in comps_ports.items():
        combined_ports = []
        for ports_per_topic in ports_per_direction.values():
            combined_ports += [port_info for port_info in ports_per_topic.values()]

        result.append({
            'name': comp_name,
            'ports': combined_ports
        })

    return result


def build_database_from_events(traces):

    in_events = parse_input_events(traces)

    # determine total time range
    tstamps = [e['timestamp'] for e in in_events]
    min_tstamp = min(tstamps)  # in us
    max_tstamp = max(tstamps)  # in us

    # convert input events to output event descriptors for display
    event_descs = []
    event_index = 0
    for event in in_events:
        descs = convert_input_event(event, event_index, min_tstamp)  # subtracts earliest timestamp from all events
        event_descs += descs
        event_index += 1

    event_descs = pd.DataFrame(event_descs)
    event_descs = event_descs.set_index('t').sort_index()

    categs = set(event_descs.loc[:, 'cat'].dropna().to_dict().values())

    # split event descriptors according to main categories (i.e. horizontal graph axes)
    eventdescs_by_categ = {}  # dictionary of data frames
    for categ in categs:
        df = event_descs.loc[event_descs['cat'] == categ]
        eventdescs_by_categ[categ] = df.sort_index()

    # collect component/port info
    comps_info = extract_comps_info(in_events)

    return in_events, eventdescs_by_categ, 0., (max_tstamp-min_tstamp) / 1.e3, comps_info, min_tstamp


def get_event_counts(eventdescs_by_categ, timewin):

    # get time-binned event counts per category (i.e. component or value)
    counts_by_categ = {}
    for cat in eventdescs_by_categ.keys():
        eventdescs = eventdescs_by_categ.get(cat, None)
        if eventdescs is None:
            continue
        events_per_type = pd.pivot_table(eventdescs, values='cat', index=['t'], columns=['type'],
                                         aggfunc=lambda x: 1, fill_value=0)
        counts_by_categ[cat] = events_per_type.resample(f'{round(timewin*1.e9)}ns').sum().fillna(0)

    return counts_by_categ


def resample_event_counts(counts_by_categ, timewin, n_sub=1):

    resampled_by_categ = {}
    for cat in counts_by_categ.keys():
        counts = counts_by_categ.get(cat, None)
        if counts is None:
            continue
        resampled = counts.resample(f'{round(1./n_sub * timewin*1.e9)}ns').sum().fillna(0)
        resampled_by_categ[cat] = resampled.rolling(n_sub).sum().fillna(0)

    return resampled_by_categ


def sum_events(counts_by_categ, categories, evt_types):

    # determine total counts over all categories
    total_counts = pd.DataFrame()
    for cat in categories:
        counts = counts_by_categ.get(cat, None)
        if counts is None:
            continue
        total_counts = counts.add(total_counts, axis='columns', fill_value=0).fillna(0)

    evt_types = list(set(evt_types) & set(total_counts.columns))
    total_counts = total_counts.loc[:, evt_types]

    return total_counts.sum(axis=1)


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('-traces', '--traces', action='append', required=False, default=None,
                        dest='traces', type=str, help='file from which to load component trace recording')
    parser.add_argument('-trace_dirs', '--r', action='append', required=False, default=None,
                        dest='trace_dirs', type=str, help='dirs which will be recursively searched for all trace.bin files.')
    parser.add_argument('-port', '--port', action='store', required=False, default=8060,
                        dest='port', type=int, help='port')
    args = parser.parse_args()
    assert args.traces is not None or args.trace_dirs is not None, "Either traces or trace_dirs should be set."

    traces = []
    if args.traces is not None:
        traces = args.traces

    if args.trace_dirs is not None:
        for trace_dir in args.trace_dirs:
            for path in pathlib.Path(trace_dir).rglob('trace.bin'):
                traces.append(str(path))

    # load event data from trace recording file
    print("Reading and parsing event trace recording ...")
    (parsed_events, events_by_categ,
     min_time, max_time, component_info, min_tstamp) = build_database_from_events(traces)
    db_meta_data = {'comps_info': component_info, 'time_range': [min_time, max_time]}

    # determine moving window event counts per category
    timewin_hires = 0.001
    timewin_lores = 0.1

    print("Resampling at high resolution ...")
    event_counts_hires = get_event_counts(events_by_categ, timewin=timewin_hires)
    print("Resampling at low resolution ...")
    event_counts_lores = resample_event_counts(event_counts_hires, timewin=timewin_lores, n_sub=2)

    # convert counts to densities
    print("Calculating event densities ...")
    event_densities_by_categ = {cat: counts / timewin_lores for cat, counts in event_counts_lores.items()}
    print("... preparation finished")

    print("Starting server ...")
    sequence_viewer_app.run_server(debug=True, port=args.port)
