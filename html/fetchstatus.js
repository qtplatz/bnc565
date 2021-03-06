
function updateProtocol( _this, protocol ) {

    $(_this).find( ':input[id^=PULSE\\.DELAY]' ).each( function( i ){
	this.value = protocol.pulses[ i ].delay;
    });
    
    $(_this).find( ':input[id^=PULSE\\.WIDTH]' ).each( function( i ){
	this.value = protocol.pulses[ i ].width;
    });

    $(_this).find( ':input[id^=PULSE\\.POL]' ).each( function( i ){
	this.checked = ( protocol.pulses[ i ].polarity == 'true');
    });    

    $(_this).find( ':input[id^=PULSE\\.STATE]' ).each( function( i ){
	this.checked = ( protocol.pulses[ i ].state == 1 );
    });
}

function disableProtocol( _this ) {
    $(_this).find( ':input[id=ENABLE]' ).each( function(){ this.checked = false; } );
}

function updateProtocols( protocols ) {

    var cbx = $('#switch-connect');
    var state = protocols.state == 0 ? false : true;
    
    if ( state != cbx.prop('checked') ) {
	cbx.bootstrapToggle( state ? "on" : "off" );
    }
    
    $('div #interval').find( ':input' ).each( function(){ // T_0
	this.value = protocols.interval;
    });

    $(protocols.protocol).each( function( index ){
	updateProtocol( $('div #p\\[' + index + '\\]')[0], this );
    });
}

function fetchStatus() {
    var xmlhttp;
    if (window.XMLHttpRequest) {   // code for IE7+, Firefox, Chrome, Opera, Safari
	xmlhttp = new XMLHttpRequest();
    }  else  {
	xmlhttp = new ActiveXObject("Microsoft.XMLHTTP"); // code for IE6, IE5
    }

    xmlhttp.onreadystatechange = function() {

	if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
	    var data = JSON.parse( xmlhttp.responseText );
	    updateProtocols( data.protocols );
	}
    }
    
    xmlhttp.open("GET","/dg/ctl?status.json",true);
    xmlhttp.send();
}

