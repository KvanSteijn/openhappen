<?php

declare(strict_types=1);

namespace OpenHappen\Bot;

class Request {

    const TYPE_DOMAIN = 'domain';
    const TYPE_OTHER = 'other';
    const TYPE_HASHMARK = 'hashmark';
    const TYPE_PATH = 'path';
    const TYPE_UNKNOWN = 'unknown';

    const FORMAT_ROBOTS = 'robots';
    const FORMAT_SITEMAP = 'sitemap';
    const FORMAT_CONTENT = 'content';

    protected $_isRetrieved = FALSE;

    protected $_url = NULL;
    protected $_id = NULL;
    protected $_scheme  = NULL;
    protected $_host = NULL;
    protected $_name = NULL;
    protected $_extension = NULL;
    protected $_port = NULL;
    protected $_type = NULL;
    protected $_path = NULL;
    protected $_format = NULL;
    protected $_domainURL = NULL;

    protected $_httpCode = 0;
    protected $_charset = 'UTF-8';
    protected $_content = NULL;

    public function __construct(string $url) {
        $object = self::ParseURL($url);
        $this->_url = $object->url;
        $this->_id = $object->id;
        $this->_scheme = $object->scheme;
        $this->_host = $object->host;
        $this->_name = $object->name;
        $this->_extension = $object->extension;
        $this->_port = $object->port;
        $this->_type = $object->type;
        $this->_path = $object->path;
        $this->_format = $object->format;
        $this->_domainURL = $object->domainURL;
    }

    /**
     * Custom base64 encoding. Replace unsafe url chars
     *
     * @param string $url
     * @return string
     */
    static public function Base64URLEncode($url) : string {
        return strtr(base64_encode(gzcompress($url, 9)), '+/=', '._-');
    }

    /**
     * Custom base64 decode. Replace custom url safe values with normal
     * base64 characters before decoding.
     *
     * @param string $url
     * @return string
     */
    static public function Base64URLDecode($url) : string {
        return gzuncompress(base64_decode(strtr($url, '._-', '+/=')));
    }

    static public function ParseURL(string $url) {
        /* Create object */
        $object = new \stdClass;
        $object->url = strtolower($url);
        $object->id = self::Base64URLEncode($object->url);
        $object->scheme = NULL;
        $object->host = NULL;
        $object->port = NULL;
        $object->name = NULL;
        $object->extension  = NULL;
        $object->path = NULL;
        $object->format = self::FORMAT_CONTENT;
        $object->domainURL = NULL;

        /* Check if url is hashmark */
        $resultHashmark = (substr($object->url, 0, 1) === '#');
        $resultPath = (substr($object->url, 0, 1) === '/');
        if ($resultHashmark || $resultPath) {
            /* Get delimiter */
            if ($resultHashmark) {
                $delimiter = '#';
                $validType = self::TYPE_HASHMARK;
            } elseif ($resultPath) {
                $delimiter = '/';
                $validType = self::TYPE_PATH;
            }

            /* Check hash mark */
            $explURL = explode($delimiter, $object->url);
            if (!empty($explURL)) {
                if (!empty($explURL[1])) {
                    /* Set type */
                    $object->type = $validType;

                    /* Check if type is page */
                    if ($validType === self::TYPE_PATH) {

                        /* Check if page is sitemap or robots.txt */
                        $indexURL = count($explURL) - 1;
                        if ($indexURL > 0 && !empty($explURL[$indexURL])) {
                            /* Set last item of the explode */
                            $explLast = $explURL[$indexURL];

                            /* Check if page is robots.txt */
                            $robotsName = 'robots.txt';
                            $robotsLength = -1 * strlen($robotsName);
                            if (substr($object->url, $robotsLength) === $robotsName && $explLast === $robotsName) {
                                $object->format = self::TYPE_PAGE_ROBOTS;

                            /* Check if page is sitemap */
                            } elseif (preg_match('/^sitemap[a-z0-9_]+\.xml$/i', $explLast)) {
                                $object->format = self::TYPE_PAGE_SITEMAP;
                            }
                        }
                    }
                } else {
                    $object->type = self::TYPE_UNKNOWN;
                }
            } else {
                $object->type = self::TYPE_UNKNOWN;
            }

            /* Return objecf */
            return $object;
        }

        /* Set some data */
        $object->scheme = parse_url($object->url, PHP_URL_SCHEME);
        $object->port = parse_url($object->url, PHP_URL_PORT);

        /* Check if host is NULL */
        $object->host = parse_url($object->url, PHP_URL_HOST);
        if ($object->host == NULL) {
            $object->type = self::TYPE_UNKNOWN;
            return $object;
        }

        /* Check if host is valid domain name */
        $domain_validation = '/((http|https)\:\/\/)?[a-zA-Z0-9\.\/\?\:@\-_=#]+\.([a-zA-Z0-9\&\.\/\?\:@\-_=#])*/';
        if (preg_match("$domain_validation", $object->host)) {
            /* Host is domain */
            $object->type = self::TYPE_DOMAIN;

            /* Set some data */
            $explHost = explode('.', $object->host);
            $object->extension = array_pop($explHost);
            $object->name = implode('.', $explHost);
        } else {
            /* Hostname is IP */
            $object->name = $object->host;
            $object->type = self::TYPE_OTHER;
        }

        /* Set path, based on raw path */
        $path = parse_url($url, PHP_URL_PATH);
        if ($path !== NULL) {
            /* Check if page is sitemap or robots.txt */
            $explPath = explode($path, '/');
            $indexPath = count($explPath) - 1;
            if ($indexPath > 0 && !empty($explURL[$indexPath])) {
                /* Set last item of the explode */
                $explLast = $explURL[$indexPath];

                /* Check if page is robots.txt */
                $robotsName = 'robots.txt';
                $robotsLength = -1 * strlen($robotsName);
                if (substr($object->url, $robotsLength) === $robotsName && $explLast === $robotsName) {
                    $object->format = self::TYPE_PAGE_ROBOTS;

                /* Check if page is sitemap */
                } elseif (preg_match('/^sitemap[a-z0-9_]+\.xml$/i', $explLast)) {
                    $object->format = self::TYPE_PAGE_SITEMAP;
                }
            }

            /* Set path */
            $object->path = $path;
        } else {
            $object->path = '/';
        }

        /* Set simple URL */
        $object->domainURL = $object->scheme . '://' . $object->host . '/';

        /* Return object */
        return $object;
    }

    public function getUserAgent() {
        return 'Mozilla/5.0 (compatible; OpenHappenBot/0.1; +https://github.com/KvanSteijn/openhappen)';
    }

    public function retrieve() : array {
        /* Check if request is already retrieved */
        if ($this->_isRetrieved) {
            return [TRUE, ''];
        }

        /* Init CURL */
        $ch = curl_init($this->_url);
        curl_setopt($ch, CURLOPT_USERAGENT, $this->getUserAgent());
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, FALSE);
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, FALSE);
        curl_setopt($ch, CURLOPT_MAXREDIRS, 10);
        curl_setopt($ch, CURLOPT_FOLLOWLOCATION, TRUE);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 60);
        curl_setopt($ch, CURLOPT_HTTPHEADER, [
            'X-Cookies-Accepted: 1'
        ]);

        /* Get HTML page */
        $this->_content = curl_exec($ch);
        if (curl_error($ch)) {
            return [FALSE, curl_error($ch)];
        }

        /* Get the status code */
        $this->_httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);

        /* Get charset */
        $charset = curl_getinfo($ch, CURLINFO_CONTENT_TYPE);
        if (preg_match('~charset=([-a-z0-9_]+)~i', $charset, $matches)) {
            $this->_charset = $matches[1];
        }

        /* Get status */
        $status = FALSE;
        $message = '';
        switch ($this->_httpCode) {
            case 200:
            case 301:
            case 202:
                $status = TRUE;
                break;
            default:
                $message = 'Unsupported HTTP code: ' . $this->_httpCode;
                break;
        }

        /* Close CURL */
        curl_close($ch);

        /* Data is retrieved */
        $this->_isRetrieved = TRUE;

        /* Return status */
        return [$status, $message];
    }

    public function getURL() {
        return $this->_url;
    }

    public function getId() {
        return $this->_id;
    }

    public function getHost() {
        return $this->_host;
    }

    public function getPath() {
        return $this->_path;
    }

    public function getExtension() {
        return $this->_extension;
    }

    public function getDomainURL() {
        return $this->_domainURL;
    }

    public function isContent() {
        return $this->_format === self::FORMAT_CONTENT;
    }

    public function getContent($encoding = 'UTF-8') {
         /* Check if content is empty */
        if ($this->_content === NULL) {
            return [NULL, 'Content is empty'];
        }

        /* Check if charset is the same as encoding; If not convert content */
        $content = $this->_content;
        if ($this->_charset !== $encoding) {
            mb_internal_encoding($encoding);
            $content = iconv($encoding, 'UTF-8//IGNORE', $content);
        }

        /* Success */
        return [$content, ''];
    }

    public function convertToSmartDOM(string $type = SmartDOM::TYPE_PAGE, $encoding = 'UTF-8') : array {
        /* Check if content is empty */
        list($content, $message) = $this->getContent($encoding);
        if ($content === NULL) {
            return [NULL, $message];
        }

        /* Return smart DOM object */
        $smartDOM = new SmartDOM($type);
        list($status, $message) = $smartDOM->load($content);
        if (!$status) {
            return [NULL, $message];
        }

        /* Success */
        return [$smartDOM, ''];
    }
}
