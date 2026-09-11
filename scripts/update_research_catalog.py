"""Refresh the bundled public CelesTrak research-related catalog (explicit manual download)."""
from pathlib import Path
import urllib.request, datetime, json

def main():
    target=Path(__file__).resolve().parents[1]/'assets/catalog'
    target.mkdir(parents=True,exist_ok=True)
    satellites={};groups={};sources=[]
    for group in ('science','resource','weather'):
        url=f'https://celestrak.org/NORAD/elements/gp.php?GROUP={group}&FORMAT=tle'
        req=urllib.request.Request(url,headers={'User-Agent':'sattrack/1.0 catalog refresh'})
        with urllib.request.urlopen(req,timeout=45) as response: raw=response.read(4000000).decode('ascii')
        lines=[line.rstrip() for line in raw.splitlines() if line.strip()]
        if len(lines)%3: raise ValueError('Invalid TLE response for '+group)
        count=0
        for i in range(0,len(lines),3):
            name,a,b=lines[i:i+3]
            if not(a.startswith('1 ') and b.startswith('2 ') and len(a)==69 and len(b)==69):raise ValueError('Invalid TLE record')
            for line in (a,b):
                if sum(int(c) if c.isdigit() else 1 if c=='-' else 0 for c in line[:68])%10!=int(line[68]):raise ValueError('Bad TLE checksum')
            if not a[2:7].isdigit():raise ValueError('Catalog requires a propagator supporting extended NORAD IDs')
            satellites[a[2:7]]=(name,a,b);count+=1
        groups[group]=count;sources.append(url)
        print(group,count,flush=True)
    now=datetime.datetime.now(datetime.timezone.utc)
    # Write only after every feed validates, preserving the previous snapshot on failure.
    body='\n'.join('\n'.join(satellites[key]) for key in sorted(satellites))+'\n'
    (target/'research.tle').write_text(body,encoding='ascii')
    (target/'epoch.txt').write_text(now.strftime('%Y %m %d %H %M %S\n'),encoding='ascii')
    (target/'manifest.json').write_text(json.dumps({'retrieved_utc':now.isoformat(),'sources':sources,'group_counts':groups,'unique_satellites':len(satellites)},indent=2)+'\n')
    print('Unique satellites:',len(satellites))
if __name__=='__main__':main()
