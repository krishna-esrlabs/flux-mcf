""""
Copyright (c) 2024 Accenture
"""
import sys
import tkinter as tk
import tkinter.font as tkFont
import tkinter.ttk as ttk

import mcf_python_path.mcf_paths
from mcf import RecordReader

try:
    import viewer_plugin as vp
except ImportError:
    vp = None


class RecordListView():

    def __init__(self, reader, parent):
        self.tree = None
        self.reader = reader
        self.parent = parent
        self.position = 0
        self.column_header = [
            '#',
            'Time',
            'dT',
            'Topic',
            'Type',
            'Id',
            'Value',
            'Value Size (msgpack)',
            'Extmem Size',
            'Extmem Size Compr',
            'Extmem Present',
            'Extmem Compressed'
        ]
        self.row_height = tkFont.Font().metrics('linespace')
        self._setup_widgets()
        self._build_tree()


    def filter(self, text):
        self.reader.index(lambda record: record.topic.startswith(text))
        self._populate_tree()


    def _setup_widgets(self):
        container = ttk.Frame(self.parent)
        container.pack(fill='both', expand=True)

        style = ttk.Style()
        style.configure('RecordView.Treeview', rowheight=self.row_height)

        self.tree = ttk.Treeview(columns=self.column_header, show='headings', style='RecordView.Treeview')

        self.scrollbar = ttk.Scrollbar(orient="vertical", command=lambda *args: self._scroll(*args))

        self.tree.bind( "<Configure>", lambda event: self._resize_tree())
        self.tree.bind( "<<TreeviewSelect>>", lambda event: self._select())

        self.tree.grid(column=0, row=0, sticky='nsew', in_=container)

        self.scrollbar.grid(column=1, row=0, sticky='ns', in_=container)

        container.grid_columnconfigure(0, weight=1)
        container.grid_rowconfigure(0, weight=1)


    def _select(self):
        item = self.tree.focus()
        idx = self.tree.index(item)
        record = self.reader.records(self.position+idx, self.position+idx+1)[0]
        extmem = self.reader.get_extmem(record)
        if vp is not None:
            vp.view(record.topic, record.typeid, record.valueid, record.id, record.value, extmem)


    def _build_tree(self):
        for col in self.column_header:
            self.tree.heading(col, text=col.title(), anchor='w')
        self.tree.column(0, width=tkFont.Font().measure('00000'), stretch=False)
        self.tree.column(1, width=tkFont.Font().measure('00000000.000'), stretch=False)
        self.tree.column(2, width=tkFont.Font().measure('000.000'), stretch=False)
        self.tree.column(3, width=tkFont.Font().measure('o'*20), stretch=False)
        self.tree.column(4, width=tkFont.Font().measure('o'*20), stretch=False)
        self.tree.column(5, width=tkFont.Font().measure('0'*20), stretch=False)
        self.tree.column(6, width=tkFont.Font().measure('o'*30), stretch=False)
        self.tree.column(7, width=tkFont.Font().measure(self.column_header[7]), stretch=False)
        self.tree.column(8, width=tkFont.Font().measure(self.column_header[8]), stretch=False)
        self.tree.column(9, width=tkFont.Font().measure(self.column_header[9]), stretch=False)
        self.tree.column(10, width=tkFont.Font().measure(self.column_header[10]), stretch=False)


    def _resize_tree(self):
        self.max_visible_items = self.tree.winfo_height() // self.row_height - 1
        self._populate_tree()


    def _scroll(self, cmd, value, unit=None):
        if cmd == 'scroll':
            if unit == 'units':
                self.position += int(value)
            elif unit == 'pages':
                self.position += int(value)*self.max_visible_items
        elif cmd == 'moveto':
            self.position = int(self.reader.index_size()*float(value))
        self._populate_tree()


    def _clear_tree(self):
        for c in self.tree.get_children():
            self.tree.delete(c)


    def _populate_tree(self):
        if self.position > self.reader.index_size()-self.max_visible_items:
            self.position = self.reader.index_size()-self.max_visible_items
        if self.position < 0:
            self.position = 0

        self._clear_tree()
        if self.reader.index_size() > 0:
            self.scrollbar.set(self.position/self.reader.index_size(), (self.position+self.max_visible_items)/self.reader.index_size())
        else:
            self.scrollbar.set(0, 1)

        if self.position > 0:
            # get one more in the beginning so we can calculate dT
            read_start = self.position - 1
        else:
            read_start = self.position

        records = self.reader.records(read_start, self.position + self.max_visible_items)
        time_last = None
        for idx, record in enumerate(records):
            time = record.timestamp / 1000

            if time_last is not None:
                dt = '{:.3f}'.format(float(time)-time_last)
            else:
                dt = '-'
            time_last = float(time)

            value_trunc_length = 200
            value = str(record.value)
            if len(value) > value_trunc_length:
                value = value[0:value_trunc_length]+'<<<truncated>>>'

            if read_start+idx >= self.position:
                self.tree.insert('', 'end',
                    values=[
                        read_start+idx,
                        '{:.3f}'.format(time),
                        dt, record.topic,
                        record.typeid,
                        record.valueid,
                        value,
                        record.value_size,
                        record.extmem_size,
                        record.extmem_size_compressed,
                        record.extmem_present,
                        record.extmem_present and record.extmem_size_compressed != 0
                    ]
                )


def build_window(window, reader):
    header = tk.Entry(window)
    header.pack(fill='x')
    header.focus()
    viewer = RecordListView(reader, window)

    timeout = [None]
    def onkey(event, timeout):
        if timeout[0] is not None:
            header.after_cancel(timeout[0])
        timeout[0] = header.after(500,
            lambda: viewer.filter(header.get()))

    header.bind( "<Key>", lambda event: onkey(event, timeout))


if __name__ == "__main__":

    if len(sys.argv) > 1:
        infile = sys.argv[1]

        reader = RecordReader()
        reader.open(infile)
        reader.index()

        window = tk.Tk()
        window.title("MCF Record Viewer")
        window.geometry("1700x600")
        build_window(window, reader)
        window.mainloop()

        reader.close()
