function findProtocol( json, protoId, elm )
{
    var protoData = {
	index : 0
	, pulses : []
    };

    protoData[ "index" ] = protoId;
    var pulse = { delay : 0.0, width : 0.0, polarity : false, state : 0 };

    $(elm).find( ':input[id^=PULSE]' ).each( function( i ){

	if ( this.id.indexOf( "DELAY" ) >= 0 ) {
	    pulse.delay = this.value;
	}

	if ( this.id.indexOf( "WIDTH" ) >= 0 ) {
	    pulse.width = this.value;
	}

	if ( this.id.indexOf( "POL" ) >= 0 ) {
	    pulse.polarity = this.checked;
	}

	if ( this.id.indexOf( "STATE" ) >= 0 ) {
	    pulse.state = this.checked;
	    // console.log( "pulse " + JSON.stringify( pulse ) );
	    protoData.pulses.push( jQuery.extend( true, {}, pulse ) );
	}
    });

    // console.log( "prtoData: " + JSON.stringify( protoData ) );    

    json.protocol.push( protoData );
}

function commitData()
{
    var json = {
	protocols : {
	    interval : 1000
	    , state : 1
	    , protocol : []	    
	}
    };

    var interval = $( '#interval #interval' ).val();
    json.protocols.interval = interval;

    $("div [id^=p\\[]").each( function( index ){
	var protoId = $(this).attr( 'data-index' );
	console.log( "protocol: " + this.id + "; " + protoId );
	findProtocol( json.protocols, protoId, this );
    });

    // console.log( "commitData: " + JSON.stringify( json ) );
    
    var xmlhttp=new XMLHttpRequest();
    xmlhttp.onreadystatechange=function() {
	if (xmlhttp.readyState==4 && xmlhttp.status==200) {
	    document.getElementById("txtHint").innerHTML=xmlhttp.responseText;
	}
    }

    xmlhttp.open("POST","/dg/ctl?commit.json=" + JSON.stringify( json ), true);
    xmlhttp.send();
}
