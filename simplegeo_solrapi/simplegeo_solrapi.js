(function(SimpleGeoMap){
  var tids = [], selectedTids = {}, dialog, staticQuery = {},
  // Skane specific
  from, to,
  // End skane specific
  urlRepresentation = function(tidList) {
    var ti, rep = [], name;
    rep.push('tags=' + escape(tidList.join(',')));
    for (name in staticQuery) {
      rep.push(name + '=' + escape(staticQuery[name]));
    }
    return rep.join('/');
  },
  tidFilter = function (tid, term) {
    var item = $('<li></li>'),
      tag = $('<a></a>').text(term.name).appendTo(item).click(function() {
      var ti, tidList = [];
      if (typeof selectedTids[tid] !== 'undefined') {
        $(tag).addClass('active');
        delete selectedTids[tid];
      }
      else {
        $(tag).removeClass('active');
        selectedTids[tid] = term;
      }
      for (ti in selectedTids) {
        tidList.push(ti);
      }
      SimpleGeoMap.urlRepresentation(urlRepresentation(tidList));
      SimpleGeoMap.updateMarkers(true);
    });
    if (typeof selectedTids[tid] !== 'undefined') {
      $(tag).addClass('active');
    }
    item.appendTo($(source.dialog).children('ul'));
  },
  source = {
    title: Drupal.t('Filter'),
    init: function(hash) {
      var frag, pt;

      for (frag in hash) {
        pt = hash[frag].split('=');
        pt[1] = unescape(pt[1]);
        switch (pt[0]) {
          case 'tags':
            $.each(pt[1].split(','), function(idx, tid) {
              selectedTids[tid] = tid;
            });
            break;
          // Skane-specific
          case 'from':
            from = pt[1];
            break;
          case 'to':
            to = pt[1];
            break;
          // End skane specific
          default:
            staticQuery[pt[0]] = pt[1];
            break;
        }
      }
      return $('<div id="simplegeomap-tag-wrapper"><ul id="simplegeomap-tag-filters" class="dialog"></ul></div>');
    },
    query: function(data) {
      var statType;
      data.facets = 'tid';
      data.facet_limit = 60;
      data.query = '';
      for (statType in staticQuery) {
        if (statType == 'text') {
          data.query += staticQuery[statType];
        }
        else {
          data.query += ' ' + statType + ':' + staticQuery[statType];
        }
      }
      for (tid in selectedTids) {
        data.query += ' tid:' + tid;
      }

      // Skane specific
      if (from && to) {
        from += 'T00:00:00Z';
        to += 'T00:00:00Z';
        data.query += ' dm_starts:[' + from + '/DAY TO ' + to + '/DAY]';
      }
      // End skane specific

      return Drupal.settings.simpleGeoMap.solrapi.uri;
    },
    result: function(json, pushMarker) {
      var tid;
      if (typeof json['items'] !== 'undefined') {
        tids = json.facets.tid;
        $(source.dialog).children('ul').empty();
        for (tid in tids) {
          tidFilter(tid, tids[tid]);
        }
        $.each(json.items, function (i, marker) {
          pushMarker('item', marker);
        });
      }
    }
  };
  SimpleGeoMap.addSource('solrapi', source);
})(SimpleGeoMap);