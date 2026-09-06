%
% SDF (Self-Describing Format) MatLab reader
% Copyright (c) 2013-2016, SDF Development Team
%
% Distributed under the terms of the BSD 3-clause License.
% See the LICENSE file for details.
%

%% Reading functions

function constants = ReadNameVal(dir, file)
  if (nargin < 2)
    file = 'const.status'
  end
  if (nargin < 1)
    dir = './Data'
  end

  name = strcat(dir, '/', file);
  constants.file = name;
  fid = fopen(name);
  if fid ~= -1
    tline = fgetl(fid);
    while ischar(tline)
      content = parse_name_val(tline);
      if isfield(content, 'n') && isfield(content, 'v')
        constants.(content.n) = content.v;
      end
      tline = fgetl(fid);
    end
  end
end


function nv = parse_name_val(str, delim)
  if nargin < 2
    delim = '=';
  end

  parts = strsplit(str, delim);
  if size(parts, 2) == 2
    nv.n = char(strtrim(parts(1)));
    val = strtrim(parts(2));
    vald = str2double(val);
    if vald == vald
      nv.v = vald;
    end
  else
    nv = 0;
  end
end
