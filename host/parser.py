"""
parser.py
Parses raw UART lines into structured Reading objects.
"""
from dataclasses import dataclass

@dataclass
class Reading:
    """Used to store data"""
    date: str
    time: str
    temp_c: float
    temp_err: bool # temperature clamp
    light_pct: int
    light_err: bool # light clamp

def check_err(value):
    """
    Check the value of the key in the data, if it is clamp error
    return value and clamp status
    """
    if(value.endswith("_ERR")):
        return value.split("_ERR")[0],True
    else:
        return value,False

def parse_line(line):
    """
    input data from uart, 
    valid data, return the parsed data.
    invalid data, return None
    """
    try:
        required_keys = ['DATE', 'TIME', 'TEMP', 'LIGHT']
        parts = line.split(',')
        data={}
        for part in parts:
            result = part.split(':',1)
            if len(result) != 2:
                continue         
            data[result[0]] = result[1]

        if all(key in data for key in required_keys):
            temp_value,temp_err=check_err(data['TEMP'])
            light_value,light_err=check_err(data['LIGHT'])

            data['TEMP'] = float(temp_value)
            data['LIGHT'] = int(light_value)
            data['TEMP_ERR'] = temp_err
            data['LIGHT_ERR'] = light_err

            r = Reading(
                date=data['DATE'],
                time=data['TIME'],
                temp_c=data['TEMP'],
                temp_err=data['TEMP_ERR'],
                light_pct=data['LIGHT'],
                light_err=data['LIGHT_ERR'],)

            return r
        else:
            return None
    except ValueError:
        return None
    