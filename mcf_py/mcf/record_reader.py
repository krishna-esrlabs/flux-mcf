"""
Read data from mcf record (.bin file).

Example usage with indexing:

    record_reader = RecordReader()
    record_reader.open(record_path)
    record_reader.index(filter_function)

    for record in record_reader.records(custom_start_idx, custom_end_idx):
        # process record

Example usage without indexing (can be considerably faster than with indexing):

    record_reader = RecordReader()
    record_reader.open(record_path)
    record_generator = record_reader.records_generator(filter_function)

    for record in record_generator:
        # process record

Copyright (c) 2024 Accenture
"""
import msgpack
import zlib

class RecordReader:

    class Record:
        def __init__(self):
            self.timestamp = None
            self.topic = None
            self.typeid = None
            self.valueid = None
            self.value = None
            self.value_size = None
            self.value_index = None
            self.extmem_size = None
            self.extmem_index = None
            self.extmem_present = None
            self.extmem_size_compressed = 0

    def __init__(self):
        pass

    def open(self, filename):
        self.file = open(filename, 'rb')

    def index(self, filter=None, unpack_value=False):
        self._index = []
        self.file.seek(0, 0)
        unpacker = msgpack.Unpacker(self.file, raw=False)
        unpacker_file_start_pos = self.file.tell()
        while True:
            try:
                idx = unpacker_file_start_pos + unpacker.tell()

                p_header = unpacker.unpack()

                value_start = unpacker.tell()
                value = unpacker.unpack() if unpack_value else unpacker.skip()
                value_size = unpacker.tell() - value_start

                m_header = unpacker.unpack()
                extmem_size = m_header[0]
                extmem_present = m_header[1]
                if len(m_header) > 2:
                    extmem_size_compressed = m_header[2]
                else:
                    extmem_size_compressed = 0

                extmem_start = unpacker_file_start_pos + unpacker.tell()

                if extmem_present:
                    # skip over extmem
                    size = extmem_size if extmem_size_compressed == 0 else extmem_size_compressed
                    self.file.seek(unpacker_file_start_pos + unpacker.tell() + size, 0)
                    unpacker = msgpack.Unpacker(self.file, raw=False)
                    unpacker_file_start_pos = self.file.tell()

                record = RecordReader.Record()
                record.timestamp = p_header[0]
                record.topic = p_header[1]
                record.typeid = p_header[2]
                record.valueid = p_header[3]
                record.value_size = value_size
                record.value_index = value_start
                record.extmem_size = extmem_size
                record.extmem_index = extmem_start
                record.extmem_present = extmem_present
                record.extmem_size_compressed = extmem_size_compressed
                record.value = value if unpack_value else None

                if filter is None or filter(record):
                    self._index.append(idx)

            except msgpack.exceptions.OutOfData:
                break

    def index_size(self):
        return len(self._index)

    def records(self, start_idx, end_idx):
        records = []
        if start_idx < 0 or start_idx >= len(self._index) or end_idx < start_idx:
            return records
        for idx in range(start_idx, end_idx):
            if idx >= len(self._index):
                break
            self.file.seek(self._index[idx], 0)
            unpacker = msgpack.Unpacker(self.file, raw=False)
            unpacker_file_start_pos = self.file.tell()
            try:
                p_header = unpacker.unpack()

                value_start = unpacker.tell()
                value = unpacker.unpack()
                value_size = unpacker.tell() - value_start

                m_header = unpacker.unpack()
                extmem_size = m_header[0]
                extmem_present = m_header[1]
                if len(m_header) > 2:
                    extmem_size_compressed = m_header[2]
                else:
                    extmem_size_compressed = 0
                extmem_start = unpacker_file_start_pos + unpacker.tell()

                if extmem_present:
                    # skip over extmem
                    size = extmem_size if extmem_size_compressed == 0 else extmem_size_compressed
                    self.file.seek(size, 1)

                record = RecordReader.Record()
                record.timestamp = p_header[0]
                record.topic = p_header[1]
                record.typeid = p_header[2]
                record.valueid = p_header[3]
                record.value = value
                record.value_size = value_size
                record.extmem_size = extmem_size
                record.extmem_index = extmem_start
                record.extmem_present = extmem_present
                record.extmem_size_compressed = extmem_size_compressed
                records.append(record)

            except msgpack.exceptions.OutOfData:
                break
        return records

    def records_generator(self, filter=None):
        self.file.seek(0, 0)
        unpacker = msgpack.Unpacker(self.file, raw=False)
        unpacker_file_start_pos = self.file.tell()
        while True:
            try:
                p_header = unpacker.unpack()

                value_start = unpacker.tell()
                value = unpacker.unpack()
                value_size = unpacker.tell() - value_start

                m_header = unpacker.unpack()
                extmem_size = m_header[0]
                extmem_present = m_header[1]
                if len(m_header) > 2:
                    extmem_size_compressed = m_header[2]
                else:
                    extmem_size_compressed = 0
                extmem_start = unpacker_file_start_pos + unpacker.tell()

                if extmem_present:
                    size = extmem_size if extmem_size_compressed == 0 else extmem_size_compressed
                    # skip over extmem
                    self.file.seek(
                        unpacker_file_start_pos + unpacker.tell() + size, 0)
                    unpacker = msgpack.Unpacker(self.file, raw=False)
                    unpacker_file_start_pos = self.file.tell()

                record = RecordReader.Record()
                record.timestamp = p_header[0]
                record.topic = p_header[1]
                record.typeid = p_header[2]
                record.valueid = p_header[3]
                record.value_size = value_size
                record.extmem_size = extmem_size
                record.extmem_index = extmem_start
                record.extmem_present = extmem_present
                record.extmem_size_compressed = extmem_size_compressed
                record.value = value

                if filter is None or filter(record):
                    yield record

            except msgpack.exceptions.OutOfData:
                break

    def get_extmem(self, record):
        if record.extmem_present:
            self.file.seek(record.extmem_index, 0)
            if record.extmem_size_compressed != 0:
                data = zlib.decompress(
                    self.file.read(record.extmem_size_compressed),
                    bufsize=record.extmem_size)
            else:
                data = self.file.read(record.extmem_size)
            return data
        else:
            return None

    def close(self):
        self.file.close()
