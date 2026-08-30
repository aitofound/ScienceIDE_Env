%
% SDF (Self-Describing Format) MatLab reader
% Copyright (c) 2013-2016, SDF Development Team
%
% Distributed under the terms of the BSD 3-clause License.
% See the LICENSE file for details.
%

function q = GetConstantSDF(h);

global block;

fseek(h.fid, block.block_start + h.block_header_length, 'bof');

typestring = 'float64';

if block.datatype == h.DATATYPE.REAL4
    typestring = 'float32';
elseif block.datatype == h.DATATYPE.REAL8
    typestring = 'float64';
elseif block.datatype == h.DATATYPE.INTEGER4
    typestring = 'int32';
elseif block.datatype == h.DATATYPE.INTEGER8
    typestring = 'int64';
end

q = fread(h.fid, 1, typestring);
