%
% SDF (Self-Describing Format) MatLab reader
% Copyright (c) 2011-2016, SDF Development Team
%
% Distributed under the terms of the BSD 3-clause License.
% See the LICENSE file for details.
%

function q = GetPlainVariableSDF(h);

global block;

fseek(h.fid, block.block_start + h.block_header_length, 'bof');

mult = fread(h.fid, 1, 'float64');
units = deblank(strtrim(char(fread(h.fid, h.ID_LENGTH, 'uchar'))'));
block.mesh_id = deblank(strtrim(char(fread(h.fid, h.ID_LENGTH, 'uchar'))'));
npts = fread(h.fid, block.ndims, 'int32');
stagger = fread(h.fid, 1, 'int32');

if block.datatype == h.DATATYPE.REAL4
    typestring = 'single';
elseif block.datatype == h.DATATYPE.REAL8
    typestring = 'double';
elseif block.datatype == h.DATATYPE.INTEGER4
    typestring = 'int32';
elseif block.datatype == h.DATATYPE.INTEGER8
    typestring = 'int64';
end

offset = block.data_location;

tagname = 'data';
block.map = memmapfile(h.filename, 'Format', ...
        {typestring npts' tagname}, 'Offset', offset, ...
        'Repeat', 1, 'Writable', false);
q.(tagname) = block.map.data.(tagname);
