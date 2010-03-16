// $Id$
//Public function
var simplegeoMapNewMarker;

(function ($){ // Create local scope.
  var map, mapElement, mapWrapper, cluster,
  oldCenter = false, padding = 256, ajax, infoWindow, filterInputs, filters,
  filterCookie, enabledFilters = [], loader, mapState = 1, smallZoomControl,
  largeZoomControl, helpBox, maxZoom = 17, minZoom = 7;

  function newMarker(type, markerLocation, clusterCount, clusterBounds, markerId, title, map) {
    var marker, icon, iconInfo;

    function getIconSize(clusterCount, type) {
      if (clusterCount <= 9) {
        return {iconSize: [21,21], iconAnchor: [10,10], labelOffset: {left: -9, top: -5}};
      }
      else if (clusterCount <= 49) {
        return {iconSize: [39,30], iconAnchor: [19,19], labelOffset: {left: -19, top: -7}};
      }
      else if (clusterCount <= 99) {
        return {iconSize: [49,49], iconAnchor: [24,24], labelOffset: {left: -24, top: -10}};
      }
      else {
        return {iconSize: [59,59], iconAnchor: [29,29], labelOffset: {left: -29, top: -10}};
      }
    }

    function getClusterMarker(clusterCount, type) {
      icon = new GIcon();
      iconInfo = getIconSize(clusterCount, type);
      icon.image = Drupal.settings.simpleGeoTileserviceMap.imagePath + '/black_' + iconInfo.iconSize[0] + '.png';
      icon.iconSize = new GSize(iconInfo.iconSize[0], iconInfo.iconSize[1]);
      icon.iconAnchor = new GPoint(iconInfo.iconAnchor[0], iconInfo.iconAnchor[1]);

      var opts = {
        "icon": icon,
        "clickable": true,
        "labelText": clusterCount.toString(),
        "labelOffset": new GSize(iconInfo.labelOffset.left, iconInfo.labelOffset.top),
        "labelClass": 'marker-label marker-label-' + iconInfo.iconSize[0],
        "title": title
      };
      return new LabeledMarker(markerLocation, opts);
    }

    function getSingleMarker(type) {
      icon =  new GIcon();
      icon.iconSize = new GSize(15, 21);
      icon.iconAnchor = new GPoint(12, 21);
      icon.image =  Drupal.settings.simpleGeoTileserviceMap.imagePath + '/black_15.png';
      return new GMarker(markerLocation, icon);
    }

    function setupPager(markerId, marker, markerInfo) {
      var total = markerId.length,
        pagerTitle = marker.clusterCount > total ? Drupal.t('<span class="current">1</span> of latest <span class="total">!total</span>', {'!total' : total}) : Drupal.t('<span class="current">1</span> of <span class="total">!total</span>'),
        pager = $('<div class="item-list"><ul class="pager clear-block"><li class="pager-previous"><a href="#">' + Drupal.t('Previous') +'</a></li><li class="pager-next"><a href="#">' + Drupal.t('Next') + '</a></li><li>' + pagerTitle + '</li></ul></div>').prependTo(markerInfo),
        currentCount = $("span.current", pager), index = 0, oldNode = false;

      function page(index) {
        if (oldNode) {
          oldNode.hide();
        }
        else {
          $("div.node", markerInfo).hide();
        }
        // Loop from 1 to total
        index = (index % (total));
        oldNode = $("div.node:eq(" + index + ")", markerInfo).show();
        currentCount.text(index + 1);
      }

      $("span.total", pager).text(markerId.length);
      $(".pager-previous a", pager).click(function () {
        index--;
        page(index);
        return false;
      });
      $(".pager-next a", pager).click(function () {
        index++;
        page(index);
        return false;
      });

      page(index);
    }

    if (clusterCount == 1) {
      marker = getSingleMarker(type);
      // Zoom in when doubleclicking a single marker.
      // Not needed on labeledMarker (clusters) where the default event is fired.
      GEvent.addListener(marker, "dblclick", function (p) {
        map.zoomIn(p, true);
      });
    }
    else {
      marker = getClusterMarker(clusterCount, type);
    }

    GEvent.addListener(marker, "click", function () {
      var pos = map.fromLatLngToContainerPixel(markerLocation),
        infoWindowContent = $('#simplegeomap-info-content').empty(),
        markerInfo = $('<div class="marker-info marker-info-loading"></div>').appendTo(infoWindowContent);

      markerInfo.load(Drupal.settings.basePath + 'tileservice/api/info?nids=' + markerId.join(','), function () {
        $(this).removeClass('marker-info-loading');

        if ((clusterCount != 1)) {
          setupPager(markerId, marker, markerInfo);
        };

        if (map.getZoom() < maxZoom) {
          $('<a class="zoom" href="#">' + Drupal.formatPlural(clusterCount, 'Zoom in to marker', 'Zoom in to markers') + '</a>').appendTo(markerInfo).click(function() {
            map.setCenter(markerLocation, map.getBoundsZoomLevel(clusterBounds));
            return false;
          });
        }
      });

      var mapOffset = $(mapElement).offset();
      infoWindow.css({
        'top': pos.y + mapOffset.top + marker.getIcon().iconSize.height - marker.getIcon().iconAnchor.y + 2,
        'left': pos.x + mapOffset.left - marker.getIcon().iconAnchor.x
      }).show();
    });


    // Store data needed by ClusterMarker.js when creating markers.
    marker.clusterCount = clusterCount;
    marker.markerId = markerId;
    marker.clusterBounds = clusterBounds;

    return marker;
  }

  // Set public function used by ClusterMarker.js
  simplegeoMapNewMarker = newMarker;

  function removeMarkers() {
    if (cluster !== undefined) {
      cluster.removeMarkers();
    }
  }

  function updateMarkers(types, forceRefresh) {
    // No filters
    if (types.length < 1) {
      removeMarkers();
      return;
    }

    var size = map.getSize(),
    zoomLevel = map.getZoom(),
    center = map.getCenter(),
    proj = map.getCurrentMapType().getProjection(),
    pixel = proj.fromLatLngToPixel(center, zoomLevel),
    tile, topLeftPixel, topLeftTile, bottomRightPixel, bottomRightTile, url, markersArray = [];


    // Only fetch new markers if the map is moved outside the padding or
    // forceRefresh is specified.
    if (forceRefresh || !oldCenter || (Math.abs(oldCenter.x - pixel.x) >= padding) || (Math.abs(oldCenter.y - pixel.y) >= padding)) {
      tile = new GPoint(Math.floor(pixel.x/256), Math.floor(pixel.y/256));

      topLeftPixel = {x : pixel.x - (size.width/2), y: pixel.y - (size.height/2)};
      topLeftTile = new GPoint(Math.floor(topLeftPixel.x/256), Math.floor(topLeftPixel.y/256));

      bottomRightPixel = {x : pixel.x + (size.width/2), y: pixel.y + (size.height/2)};
      bottomRightTile = new GPoint(Math.floor(bottomRightPixel.x/256), Math.floor(bottomRightPixel.y/256));

      // zoom/from/to/layers
      url = Drupal.settings.basePath + 'tileservice/api/tiles';

      // Get markers

      ajax = $.ajax({
        url: url,
        data: {
          z: zoomLevel,
          x: (topLeftTile.x - 1),
          y: (topLeftTile.y - 1),
          layers: types.join(','),
          width: bottomRightTile.x - topLeftTile.x + 2,
          height: bottomRightTile.y - topLeftTile.y + 2
        },
        dataType: 'json',
        beforeSend: function () {
          // Abort previous requests;
          if (typeof ajax !== 'undefined') {
            ajax.abort();
          }
          loader.show();
        },
        success: function (json) {
          $.each(types, function (j, type) {
            var i, u, clusterBounds;
            if (typeof json[type] !== 'undefined') {
              for (i=0; i<json[type].length; i++) {
                for (u=0; u<json[type][i].length; u++) {
                   clusterBounds = new GLatLngBounds();
                  if (json[type][i][u].NW) {
                    clusterBounds.extend(new GLatLng(json[type][i][u].NW[0], json[type][i][u].NW[1]));
                    clusterBounds.extend(new GLatLng(json[type][i][u].SE[0], json[type][i][u].SE[1]));
                  }
                  else {
                    clusterBounds.extend(new GLatLng(json[type][i][u].lat, json[type][i][u].lon));
                  };
                  markersArray.push(newMarker(type, new GLatLng(json[type][i][u].lat, json[type][i][u].lon), json[type][i][u].count, clusterBounds, json[type][i][u].nid, Drupal.t("Show items"), map));
                }
              }
            }
          });

          // Save the current center
          oldCenter = pixel;

          // Remove previous cluster.
          removeMarkers();

          // Add cluster
          cluster.addMarkers(markersArray);
          cluster.refresh();
          loader.hide();
        }
      });
    }
  }

  function setEnabledFilters() {
    // Reset array
    enabledFilters = [];
    filterInputs.each(function () {
      if (this.checked) {
        enabledFilters.push($(this).val());
      }
    });
    return enabledFilters;
  }

  function toggleSize() {
    var center = map.getCenter();
    switch (mapState) {
    case 0: // Swith to large
      mapWrapper.removeClass('small').addClass('large');
      map.removeControl(smallZoomControl);
      map.addControl(largeZoomControl);
      mapState = 1;
      break;
    case 1: // Switch to small
      $("a", this).text(Drupal.t('Enlarge map'));
      mapWrapper.addClass('small').removeClass('large');
      map.removeControl(largeZoomControl);
      map.addControl(smallZoomControl);
      mapState = 0;
      break;
    }

    map.checkResize();
    map.setCenter(center);
  }

  function closeHelpBox() {
    if (helpBox.is(':visible')) {
      helpBox.css('display', 'none');
      $.cookie('simplegeoMapTouched', '1', {expires: 9999});
    };
  }

  function addToolbar() {
    var toolbar, filterToggle, filterHTML, filterBox, sizeToggle,
    fullscreen, fullscreenToggle, searchAddress;

    function closeFilterBox(event) {
      var target = $(event.target);
      if (target.is("a.close") || !target.parents(filterBox).is('#simplegeomap-filters')) {
        filterBox.hide();
        $(document).unbind('click', closeFilterBox);
      }
    }

    toolbar = $('<ul id="simplegeomap-toolbar"></ul>').insertBefore(mapElement);

    // Add home button
    $('<li class="home"><a href="#">' + Drupal.t('Home') + '</a></li>').appendTo(toolbar).click(function() {
      setCenter();
      return false;
    });

    // Add filter control
    filterToggle = $('<li class="toggle-filter"><a href="#">' + Drupal.t('Show/Hide markers') + '</a></li>').appendTo(toolbar);
    filterHTML = '<div id="simplegeomap-filters" class="modal"><div class="wrapper">';

    $.each(filters, function (i, filter) {
      var filterEnabled = ($.inArray(filter[0], enabledFilters) !== -1);

      filterHTML += '<label class="filter-' + filter[0] + '"><img src="' + Drupal.settings.simpleGeoTileserviceMap.imagePath + '/markers/' + filter[0] + '.png"><input type="checkbox" ' + (filterEnabled ? 'checked="checked"' : '') + 'value="' + filter[0] + '"><span>' +  filter[1] + '</span></label>';
    });

    filterHTML += '<a class="close" href="#">' + Drupal.t('Close box') + '</a></div></div>';

    filterBox = $(filterHTML).insertBefore(mapElement).hide();

    $("a", filterToggle).click(function (event) {
      closeHelpBox();
      if (filterBox.is(':hidden')) {
        filterBox.show();
        $(document).bind('click', closeFilterBox);
        return false;
      }
      else {
        closeFilterBox(event);
        return false;
      }
    });

    filterInputs = filterBox.find("input");

    filterInputs.each(function () {
      $(this).click(function () {
        var filters = setEnabledFilters();
        updateMarkers(filters, true);
        // Save filters in cookie.
        $.cookie('simplegeoMapFilters', filters, {expires: 365});
      });
    });

    //Add size control
    sizeToggle = $('<li class="toggle-size"><a href="#">' + (mapState ? Drupal.t('Minimize map') : Drupal.t('Enlarge map')) + '</a></li>').appendTo(toolbar);

    sizeToggle.click(function () {
      toggleSize(this);
      $("a", this).text((mapState ? Drupal.t('Minimize map') : Drupal.t('Enlarge map')));
      $.cookie('simplegeoMapState', mapState, {expires: 365});
      return false;
    });

    // Add fullscreen control
    fullscreen = false;
    fullscreenToggle = $('<li class="toggle-fullscreen"><a href="#">' + Drupal.t('Fullscreen') + '</a></li>').appendTo(toolbar);

    fullscreenToggle.click(function () {
      var center = map.getCenter();
      if (fullscreen) {
        $("html").removeClass('fullscreen');
        $("a", this).text(Drupal.t('Fullscreen'));
        mapWrapper.replaceAll('#map-temp-wrapper');
        fullscreen = false;
      }
      else {
        $("html").addClass('fullscreen');
        $("a", this).text(Drupal.t('Leave fullscreen'));
        mapWrapper.before('<div id="map-temp-wrapper"></div>').prependTo('body');
        updateMarkers(enabledFilters, true);
        fullscreen = true;
      }
      map.checkResize();
      map.setCenter(center);
      return false;
    });

    // Add search control
    searchAddress =  $('<li class="search"></li>').appendTo(toolbar);
    map.addControl(new LookupControl(null, $("#simplegeomap-toolbar .search")));
  }

  function setCenter() {
    var zoom, center, positions, mapBounds, p;
    if (Drupal.settings.simpleGeoTileserviceMap.center) {
      center = Drupal.settings.simpleGeoTileserviceMap.center.split(" ");
      center = new GLatLng(center[0], center[1]);
    }
    else if (Drupal.settings.simple_geo_position) {
      center = Drupal.settings.simple_geo_position.split(" ");
      center = new GLatLng(center[0], center[1]);
    }
    else {
      center = new GLatLng(55.655897188968034, 12.557373046875);
    }
    if (Drupal.settings.simple_geo_area) {
      positions = Drupal.settings.simple_geo_area.split(",");

      mapBounds = new GLatLngBounds();

      if (positions) {
        for (p in positions) {
          if (positions.hasOwnProperty(p)) {
            point = positions[p].split(" ");
            mapBounds.extend(new GLatLng(point[0], point[1]));
          }
        }
      }

      zoom = map.getBoundsZoomLevel(mapBounds) + 1;
    }
    else if (Drupal.settings.simpleGeoTileserviceMap.zoom) {
      zoom = Drupal.settings.simpleGeoTileserviceMap.zoom;
    }
    else {
      zoom = 10;
    }
    map.setCenter(center, zoom);
  }

  function init() {
    var mt, i, savedMapState;
    mapWrapper = $("#large-map");

    mapElement = document.getElementById("map");
    map = new GMap2(mapElement);

    // Restricting the range of Zoom Levels
    // Get the list of map types
    mt = map.getMapTypes();
    // Overwrite the getMinimumResolution() and getMaximumResolution() methods
    for (i=0; i<mt.length; i++) {
      mt[i].getMinimumResolution = function () {
        return minZoom;
      };
      mt[i].getMaximumResolution = function () {
        return maxZoom;
      };
    }

    setCenter();
    savedMapState = $.cookie('simplegeoMapState');

    smallZoomControl = new GSmallZoomControl3D();
    largeZoomControl = new GLargeMapControl3D();

    if (savedMapState !== null && mapState !== Number(savedMapState) && Drupal.settings.simpleGeoTileserviceMap.toolbar !== false) {
      toggleSize();
    }
    else {
      map.addControl(largeZoomControl);
    }

    cluster = new ClusterMarker(map, {intersectPadding: 5});
    //cluster = new ClusterMarker(map, {intersectPadding: 5, clusteringEnabled: false});

    GEvent.addListener(map, "click", function (overlay) {
      closeHelpBox();
      if (!overlay) {
        infoWindow.hide();
      }
    });
    GEvent.addListener(map, "movestart", function () {
      closeHelpBox();
      infoWindow.hide();
    });
    GEvent.addListener(map, "moveend", function () {
      closeHelpBox();
      infoWindow.hide();
      updateMarkers(enabledFilters);
    });


    //marker Infowindow
    infoWindow = $('<div id="simplegeomap-info-window"><div id="simplegeomap-info-content"></div></div>').appendTo('body').hide();
    $('<a href="#" class="close">' + Drupal.t('Close') + '</a>').prependTo(infoWindow).click(function () {
      infoWindow.hide();
      return false;
    });

    filters = Drupal.settings.simpleGeoTileserviceMap.filters;
    filterCookie = $.cookie('simplegeoMapFilters');

    // Set initial enabled filters based on Drupal.setting, cookie or filter setting,
    if (Drupal.settings.simpleGeoTileserviceMap.enabledFilters) {
      enabledFilters = Drupal.settings.simpleGeoTileserviceMap.enabledFilters;
    }
    else if (filterCookie) {
      enabledFilters = filterCookie.split(',');
    }
    else {
      for (i=0; i < filters.length; i++) {
        if (filters[i][2]) {
          enabledFilters.push(filters[i][0]);
        }
      }
    }

    //Add loader
    loader = $('<div id="simplegeomap-loader">Loading...</div>').insertBefore(mapElement).hide();

    // Add toolbar
    if (Drupal.settings.simpleGeoTileserviceMap.toolbar !== false) {
      addToolbar();
    }

    // Helbox
    helpBox = $('#simplegeomap-help');
    if ($.cookie('simplegeoMapTouched') !== '1') {
      helpBox.show();
      $("a.close", helpBox).click(function() {
        closeHelpBox();
        return false;
      });
    }

    updateMarkers(enabledFilters);
  }

  // Load up the map.
  if (google && google.load) {
    google.load('maps', '2');
    $(document).ready(function () {
      init();
    });
  }
})(jQuery);
