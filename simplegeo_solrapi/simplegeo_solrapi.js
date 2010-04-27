(function(SimpleGeoMap){
  var tids = [], selectedTids = {}, dialog,
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
      SimpleGeoMap.urlRepresentation(tidList.join('/'));
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
      var tid;
      if (hash) {
        for (tid in hash) {
          selectedTids[hash[tid]] = hash[tid];
        }
      }
      return $('<div id="simplegeomap-tag-wrapper"><ul id="simplegeomap-tag-filters" class="dialog"></ul></div>');
    },
    query: function(data) {
      data.facets = 'tid';
      data.facet_limit = 60;
      data.query = '';
      for (tid in selectedTids) {
        data.query += ' tid:' + tid;
      }
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