"""Fetch a reproducible SatNOGS snapshot for offline use (CC BY-SA 4.0)."""
import argparse, datetime, json, pathlib, urllib.request

SOURCES = {
    'satellites': ('https://db.satnogs.org/api/satellites/?format=json',
                   ['sat_id','norad_cat_id','name','names','status','launched','deployed','operator','countries','updated','website']),
    'tle': ('https://db.satnogs.org/api/tle/?format=json',
            ['tle0','tle1','tle2','tle_source','sat_id','norad_cat_id','updated']),
    'transmitters': ('https://db.satnogs.org/api/transmitters/?format=json',
                     ['sat_id','norad_cat_id','description','downlink_low','downlink_high','mode','baud','status','updated']),
    'stations': ('https://network.satnogs.org/api/stations/?format=json',
                 ['id','name','lat','lng','altitude','status','last_seen','min_horizon','antenna','observations']),
}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--from-downloads',type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('assets/satnogs'));args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    for name,(url,fields) in SOURCES.items():
        if args.from_downloads:
            rows=json.loads((args.from_downloads/(name+'.json')).read_text(encoding='utf-8'))
        else:
            with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'sattrak'}),timeout=60) as r:rows=json.load(r)
        if not isinstance(rows,list):raise ValueError('Expected complete array from '+url)
        data=[{key:row.get(key) for key in fields} for row in rows]
        (args.output/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
        print(name,len(data))
    (args.output/'downloaded.txt').write_text(datetime.datetime.now(datetime.timezone.utc).isoformat())

if __name__=='__main__':main()
