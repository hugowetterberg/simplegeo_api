(function(SimpleGeoMap){
  var tids = [], selectedTids = {}, dialog,
  tidFilter = function (tid, term) {
    var item = $('<li></li>'),
      tag = $('<a></a>').text(term.name).appendTo(item).click(function() {
      if (typeof selectedTids[tid] !== 'undefined') {
        $(tag).addClass('active');
        delete selectedTids[tid];
      }
      else {
        $(tag).removeClass('active');
        selectedTids[tid] = term;
      }
      SimpleGeoMap.updateMarkers(true);
    });
    if (typeof selectedTids[tid] !== 'undefined') {
      $(tag).addClass('active');
    }
    item.appendTo($(source.dialog).children('ul'));
  },
  source = {
    title: Drupal.t('Filter'),
    init: function() {
      return $('<div id="simplegeomap-tag-wrapper"><ul id="simplegeomap-tag-filters" class="dialog"></ul></div>');
    },
    query: function(data) {
      data.facets = 'tid';
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