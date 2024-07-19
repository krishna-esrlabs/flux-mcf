""""
Copyright (c) 2024 Accenture
"""
import sys
import tkinter as tk
import tkinter.font as tkFont
import tkinter.ttk as ttk

import mcf_python_path.mcf_paths
from mcf import RemoteControl

try:
    import viewer_plugin as vp
except ImportError:
    vp = None


class Selection():

    def __init__(self, component_index, port_index):
        self.component_index = component_index
        self.port_index = port_index


    def __eq__(self, other):
        return type(self) == type(other) and self.component_index == other.component_index and self.port_index == other.port_index


    def __str__(self):
        return '{}/{}'.format(self.component_index, self.port_index)


class AppModel():

    def __init__(self, remote_control):
        self._remote_control = remote_control
        self._change_listeners = []
        self._selection = None


    def reload(self):
        self._info = self._remote_control.get_info()
        self._notify_listeners('content')


    def get_selection(self):
        return self._selection


    def set_selection(self, selection):
        if selection != self._selection:
            self._selection = selection
            self._notify_listeners('selection')


    def connect_port(self, component_idx, port_idx):
        self._remote_control.connect_port(component_idx, port_idx)
        self.reload()


    def disconnect_port(self, component_idx, port_idx):
        self._remote_control.disconnect_port(component_idx, port_idx)
        self.reload()


    def read_value(self, topic):
        return self._remote_control.read_value(topic)


    def write_value(self, topic, clazz, value, extmem):
        return self._remote_control.write_value(topic, clazz, value, extmem)


    def get_info(self):
        return self._info


    def add_change_listener(self, listener):
        self._change_listeners.append(listener)


    def _notify_listeners(self, event):
        for listener in self._change_listeners:
            listener(event)


class ComponentView():

    def __init__(self, app_state, parent):
        self.app_state = app_state
        self.parent = parent
        self.tree = None
        self.column_header = ['Connected', 'Direction', 'Topic']
        self.row_height = tkFont.Font().metrics('linespace')
        self._setup_widgets()
        self._build_tree()
        self.app_state.add_change_listener(lambda event: self.on_change(event))


    def on_change(self, event):
        if event == 'content':
            self._populate_tree()


    def _setup_widgets(self):
        container = ttk.Frame(self.parent)
        container.pack(side='left', fill='both', expand=True)

        style = ttk.Style()
        style.configure('ComponentView.Treeview', rowheight=self.row_height)

        self.tree = ttk.Treeview(columns=self.column_header, style='ComponentView.Treeview')

        self.scrollbar = ttk.Scrollbar(orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=self.scrollbar.set)

        self.tree.bind( "<Configure>", lambda event: self._resize_tree())
        self.tree.bind( "<<TreeviewSelect>>", lambda event: self._select())

        self.tree.grid(column=0, row=0, sticky='nsew', in_=container)

        self.scrollbar.grid(column=1, row=0, sticky='ns', in_=container)

        container.grid_columnconfigure(0, weight=1)
        container.grid_rowconfigure(0, weight=1)


    def _build_tree(self):
        for col in self.column_header:
            self.tree.heading(col, text=col.title(), anchor='w')
        self.tree.column('#0', width=200, stretch=False)
        self.tree.column(0, width=tkFont.Font().measure('Connected'), stretch=False)
        self.tree.column(1, width=tkFont.Font().measure('Direction'), stretch=False)


    def _resize_tree(self):
        self._populate_tree()


    def _get_selection(self):
        item = self.tree.focus()
        if self.tree.parent(item) == '':
            ci = self.tree.index(item)
            pi = None
        else:
            ci = self.tree.index(self.tree.parent(item))
            pi = self.tree.index(item)
        sel = Selection(ci, pi)
        return sel


    def _set_selection(self, selection):
        if selection is not None:
            comp_id = self.tree.get_children()[selection.component_index]
            if selection.port_index is not None:
                port_id = self.tree.get_children(comp_id)[selection.port_index]
                self.tree.selection_set(port_id)
            else:
                self.tree.selection_set(comp_id)


    def _select(self):
        self.app_state.set_selection(self._get_selection())


    def _clear_tree(self):
        for c in self.tree.get_children():
            self.tree.delete(c)


    def _populate_tree(self):
        self._clear_tree()
        info = self.app_state.get_info()
        for ci, comp in enumerate(info):
            comp_id = self.tree.insert('' , 'end', text=comp['name'], open=True)
            for pi, port in enumerate(comp['ports']):
                self.tree.insert(comp_id, 'end', text='#{}'.format(pi), values=(
                    'yes' if port['connected'] else 'no',
                    port['direction'], 
                    port['topic']))
        self._set_selection(self.app_state.get_selection())


class DetailsView():

    def __init__(self, app_state, parent):
        self.app_state = app_state
        self.parent = parent
        self._setup_widgets()
        self.app_state.add_change_listener(lambda event: self.on_change(event))


    def on_change(self, event):
        sel = self.app_state.get_selection();
        if sel is not None:
            comp_info = self.app_state.get_info()[sel.component_index]
            if sel.port_index is not None:
                port_info = comp_info['ports'][sel.port_index]
                self.component_var.set(comp_info['name'])
                self.port_var.set('#{}'.format(sel.port_index))
                self.topic_var.set(port_info['topic'])
                if port_info['connected']:
                    self.connection_button_var.set('Disconnect')
                else:
                    self.connection_button_var.set('Connect')
                self.connection_button.config(state='normal')
            else:
                self.component_var.set(comp_info['name'])
                self.port_var.set('')
                self.topic_var.set('')
                self.connection_button_var.set('Disconnect')
                self.connection_button.config(state='disabled')
        else:
            self.component_var.set('')
            self.port_var.set('')
            self.topic_var.set('')
            self.connection_button_var.set('Disconnect')
            self.connection_button.config(state='disabled')


    def _toggle_connect(self):
        sel = self.app_state.get_selection();
        if sel is not None:
            comp_info = self.app_state.get_info()[sel.component_index]
            if sel.port_index is not None:
                port_info = comp_info['ports'][sel.port_index]
                if port_info['connected']:
                    self.app_state.disconnect_port(sel.component_index, sel.port_index)
                else:
                    self.app_state.connect_port(sel.component_index, sel.port_index)


    def _read_value(self):
        topic = self.topic_var.get()
        value = self.app_state.read_value(topic)
        self.value_text_field.delete(1.0, 'end')
        self.value_text_field.insert('end', str(value)[0:1000])

        if value is not None:
            content = value[0]
            typeid = value[1]
            extmem = value[2]
            if vp is not None:
                vp.view(topic, typeid, content, extmem)

    def _write_value(self):
        val = self.value_text_field.get(1.0, 'end')
        try:
            val = eval(val)
        except Exception as e:
            print(e)
            return
        val, typename, extmem = val

        self.app_state.write_value(self.topic_var.get(), typename, val, extmem)


    def _setup_widgets(self):
        container = ttk.Frame(self.parent, relief=tk.SUNKEN, padding=5)
        container.pack(fill='both', side='right')

        self.component_var = tk.StringVar(value='')
        self.port_var = tk.StringVar(value='')
        self.topic_var = tk.StringVar(value='')
        self.connection_button_var = tk.StringVar(value='')

        tk.Label(container, text='Component', anchor='w').pack(fill='x')
        tk.Entry(container, textvariable=self.component_var, width=200, relief=tk.SUNKEN).pack(fill='x')

        tk.Label(container, text='Port', anchor='w').pack(fill='x')
        port_container = ttk.Frame(container)
        port_container.pack(fill='x')
        tk.Entry(port_container, textvariable=self.port_var, relief=tk.SUNKEN).pack(side='left', fill='x')
        self.connection_button = tk.Button(port_container, textvariable=self.connection_button_var, state='disabled', command=lambda: self._toggle_connect())
        self.connection_button.pack(fill='x')

        tk.Label(container, text='Topic', anchor='w').pack(fill='x')
        tk.Entry(container, textvariable=self.topic_var, width=200, relief=tk.SUNKEN).pack(fill='x')

        rw_container = ttk.Frame(container)
        rw_container.pack(fill='x')
        self.read_button = tk.Button(rw_container, text='Read', command=lambda: self._read_value())
        self.read_button.pack(fill='x')
        self.write_button = tk.Button(rw_container, text='Write', command=lambda: self._write_value())
        self.write_button.pack(fill='x')

        self.value_text_field = tk.Text(container)
        self.value_text_field.pack(fill='both')


if __name__ == "__main__":

    if len(sys.argv) > 2:
        ip = sys.argv[1]
        port = sys.argv[2]

        remote_control = RemoteControl()
        if not remote_control.connect(ip, port):
            exit()

        window = tk.Tk()
        window.title("MCF Remote Viewer")
        window.geometry("800x600")

        app_state = AppModel(remote_control)
        ComponentView(app_state, window)
        DetailsView(app_state, window)

        app_state.reload()

        window.mainloop()

        remote_control.disconnect()
